// SPDX-License-Identifier: MIT
// PID 1 starts the existing USB probe. A separate process records uname on the test SD.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "sd_run.h"  // Generated for one run: RUN_ID, PART_START and PART_SECTORS (strings).

static int read_text(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return -1;
    ssize_t n = read(fd, buf, size);
    close(fd);
    if (n <= 0 || (size_t)n >= size)
        return -1;
    buf[n] = '\0';
    return 0;
}

static int attribute(const char *base, const char *name, const char *expected)
{
    char path[PATH_MAX], value[64];
    if (snprintf(path, sizeof(path), "%s/%s", base, name) >= (int)sizeof(path))
        return 0;
    return read_text(path, value, sizeof(value)) == 0 && strcmp(value, expected) == 0;
}

static dev_t sd_partition(void)
{
    DIR *dir = opendir("/sys/block");
    if (!dir)
        return 0;
    struct dirent *entry;
    dev_t result = 0;
    while ((entry = readdir(dir))) {
        unsigned int index, major_number, minor_number;
        char extra, disk[128], part[256], path[320], value[40];
        if (sscanf(entry->d_name, "mmcblk%u%c", &index, &extra) != 1)
            continue;
        snprintf(disk, sizeof(disk), "/sys/block/mmcblk%u", index);
        if (!attribute(disk, "device/type", "SD\n"))
            continue;
        snprintf(part, sizeof(part), "%s/mmcblk%up1", disk, index);
        if (!attribute(part, "partition", "1\n") ||
            !attribute(part, "start", PART_START "\n") ||
            !attribute(part, "size", PART_SECTORS "\n"))
            continue;
        snprintf(path, sizeof(path), "%s/dev", part);
        if (read_text(path, value, sizeof(value)) == 0 &&
            sscanf(value, "%u:%u", &major_number, &minor_number) == 2) {
            result = makedev(major_number, minor_number);
            break;
        }
    }
    closedir(dir);
    return result;
}

static int write_result(const char *file, const struct utsname *info)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return -1;
    int ok = dprintf(fd, "run=%s\nsysname=%s\nrelease=%s\nmachine=%s\ncomplete=yes\n",
                     RUN_ID, info->sysname, info->release, info->machine) > 0 && fsync(fd) == 0;
    if (close(fd) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

static int record(int control)
{
    struct utsname info;
    if (uname(&info) != 0 || (!control && strcmp(info.release, "6.12.118") != 0))
        return 1;

    dev_t device = 0;
    for (int i = 0; i < 90 && !device; ++i) {
        device = sd_partition();
        if (!device)
            sleep(1);
    }
    char work[] = "/tmp/sdprobe-XXXXXX";
    if (!device || !mkdtemp(work) || chdir(work) != 0)
        return 1;
    int mounted = 0, result = 1;
    const unsigned long flags = MS_NODEV | MS_NOSUID | MS_NOEXEC | MS_NOATIME;
    const char *marker = "card/K612-" RUN_ID "/run-id";
    const char *file = control ? "card/K612-" RUN_ID "/control.txt"
                               : "card/K612-" RUN_ID "/result.txt";
    char actual[64];
    if (mkdir("card", 0700) || mknod("partition", S_IFBLK | 0600, device))
        goto cleanup;
    if (mount("partition", "card", "vfat", flags | MS_RDONLY, NULL) != 0)
        goto cleanup;
    mounted = 1;
    if (read_text(marker, actual, sizeof(actual)) != 0 || strcmp(actual, RUN_ID "\n"))
        goto cleanup;
    if (mount("partition", "card", "vfat", flags | MS_REMOUNT, NULL) != 0)
        goto cleanup;
    if (read_text(marker, actual, sizeof(actual)) == 0 && strcmp(actual, RUN_ID "\n") == 0)
        result = write_result(file, &info) == 0 ? 0 : 1;
cleanup:
    if (result)
        perror("sd probe");
    if (mounted && umount("card") != 0)
        result = 1;
    unlink("partition");
    rmdir("card");
    if (chdir("/") == 0)
        rmdir(work);
    return result;
}

int main(int argc, char **argv)
{
    if (getpid() != 1)
        return argc == 2 && strcmp(argv[1], "--control") == 0 ? record(1) : 2;
    if (fork() == 0)
        _exit(record(0));
    execl("/usb-init", "usb-init", (char *)NULL);
    perror("exec /usb-init");
    for (;;)
        pause();
}
