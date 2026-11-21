// SPDX-License-Identifier: Apache-2.0
// Include only in the RAM diagnostic variant of Android's first-stage init.
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

namespace klimt_trial {

constexpr char kSystem[] = "/dev/block/by-name/SYSTEM";
constexpr char kTty[] = "/dev/klimt-ttyGS0";
inline std::string run_id;

inline void Send(const std::string& message) {
    const std::string line = "KLIMT-FIRST " + run_id + " " + message;
    LOG(WARNING) << line;
    std::string number;
    unsigned int maj, min;
    if (android::base::ReadFileToString("/sys/class/tty/ttyGS0/dev", &number) &&
        sscanf(number.c_str(), "%u:%u", &maj, &min) == 2) {
        if (mknod(kTty, S_IFCHR | 0600, makedev(maj, min)) && errno != EEXIST) return;
        android::base::unique_fd fd(open(kTty, O_WRONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC));
        if (fd.ok()) dprintf(fd.get(), "%s\r\n", line.c_str());
    }
}

[[noreturn]] inline void Stop(const std::string& result) {
    // Repeat the verdict so the host can open ACM after enumeration.
    for (int i = 0; i < 4; ++i) {
        Send(result);
        sleep(3);
    }
    Send("RECOVERY_REQUEST");
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
            LINUX_REBOOT_CMD_RESTART2, "recovery");
    for (;;) pause();
}

inline void Start() {
    android::base::ReadFileToString("/klimt-trial-id", &run_id);
    run_id = android::base::Trim(run_id);
    pid_t child = fork();
    if (child == 0) {
        sleep(65);
        Stop("FAIL watchdog");
    }
    if (child < 0) Stop("FAIL watchdog fork");
    Send("NATIVE_PID1_STARTED pid=" + std::to_string(getpid()));
}

inline dev_t ProtectSystem() {
    android::base::unique_fd fd(open(kSystem, O_RDONLY | O_CLOEXEC));
    struct stat st {};
    std::string uevent;
    if (!fd.ok() || fstat(fd.get(), &st) || !S_ISBLK(st.st_mode)) Stop("FAIL SYSTEM link");
    const std::string id = std::to_string(major(st.st_rdev)) + ":" + std::to_string(minor(st.st_rdev));
    if (!android::base::ReadFileToString("/sys/dev/block/" + id + "/uevent", &uevent) ||
        uevent.find("\nPARTNAME=SYSTEM\n") == std::string::npos ||
        uevent.find("\nDEVNAME=mmcblk0p18\n") == std::string::npos) Stop("FAIL SYSTEM identity " + id);
    int ro = 1, observed = 0;
    if (ioctl(fd.get(), BLKROSET, &ro) || ioctl(fd.get(), BLKROGET, &observed) || observed != 1)
        Stop("FAIL SYSTEM read-only guard");
    Send("SYSTEM_BLOCK_RO mmcblk0p18 " + id);
    return st.st_rdev;
}

[[noreturn]] inline void CheckRoot(dev_t expected) {
    struct stat root {}, executable {};
    struct statfs fs {};
    struct statvfs flags {};
    if (stat("/", &root) || root.st_dev != expected || statfs("/", &fs) ||
        fs.f_type != 0xef53 || statvfs("/", &flags) || !(flags.f_flag & ST_RDONLY))
        Stop("FAIL mounted root identity/type/flags");
    unsigned char elf[20] = {};
    android::base::unique_fd fd(open("/system/bin/init", O_RDONLY | O_CLOEXEC));
    if (!fd.ok() || fstat(fd.get(), &executable) || !S_ISREG(executable.st_mode) ||
        executable.st_dev != expected || !(executable.st_mode & 0111) ||
        read(fd.get(), elf, sizeof(elf)) != sizeof(elf) || memcmp(elf, "\177ELF", 4) ||
        elf[4] != 1 || elf[5] != 1 || elf[18] != 40 || elf[19] != 0)
        Stop("FAIL installed ARM init");
    Stop("PASS SYSTEM_ROOT ext4 ro mmcblk0p18 ARM32 /system/bin/init");
}
}  // namespace klimt_trial
