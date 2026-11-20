// SPDX-License-Identifier: MIT
// SM-T700 RAM diagnostic: send a runtime version over USB CDC ACM.
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <unistd.h>

static void send_report(int fd, const char *text)
{
    size_t left = strlen(text);
    while (left) {
        ssize_t count = write(fd, text, left);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return;
        text += count;
        left -= (size_t)count;
    }
}

int main(void)
{
    if (getpid() != 1)
        return 1;

    struct utsname kernel;
    if (uname(&kernel) != 0)
        return 1;
    const unsigned long flags = MS_NOSUID | MS_NOEXEC;
    if (mount("devtmpfs", "/dev", "devtmpfs", flags, NULL) < 0)
        perror("devtmpfs");
    if (mount("sysfs", "/sys", "sysfs", flags | MS_NODEV, NULL) < 0)
        perror("sysfs");

    for (;;) {
        char cpus[64] = "unknown";
        int cpu = open("/sys/devices/system/cpu/online", O_RDONLY | O_CLOEXEC);
        if (cpu >= 0) {
            ssize_t n = read(cpu, cpus, sizeof(cpus) - 1);
            if (n > 0) {
                cpus[n] = '\0';
                cpus[strcspn(cpus, "\n")] = '\0';
            }
            close(cpu);
        }
        char report[400];
        snprintf(report, sizeof(report),
                 "\nSM-T700 ACM RAM INIT READY\nrelease=%s\nmachine=%s\nonline=%s\nEND SM-T700\n",
                 kernel.release, kernel.machine, cpus);
        int fd = open("/dev/ttyGS0", O_WRONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            send_report(fd, report);
            close(fd);
        }
        sleep(2);
    }
}
