// SPDX-License-Identifier: Apache-2.0
// Include only in the SM-T700 internal-install variant of Android's first-stage init.
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <linux/reboot.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

#include "property-ramdisk.h"

extern "C" int __system_properties_init();

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
    sync();
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
            LINUX_REBOOT_CMD_RESTART2, "recovery");
    for (;;) pause();
}

inline void Start() {
    android::base::ReadFileToString("/klimt-trial-id", &run_id);
    run_id = android::base::Trim(run_id);
    StageTrialFile("/klimt-property.rc", "/dev/klimt-property.rc");
    StageTrialFile("/klimt-cgroups.json", "/dev/klimt-cgroups.json");
    StageTrialFile("/klimt-cgroups.original", "/dev/klimt-cgroups.original");
    Send("NATIVE_PID1_STARTED pid=" + std::to_string(getpid()));
}

inline void StartWatchdog() {
    // Fork after SwitchRoot so the child sees the installed Android property area.
    const pid_t child = fork();
    if (child == 0) {
        sleep(420);
        char boot_completed[PROP_VALUE_MAX] {};
        if (__system_properties_init() == 0 &&
            __system_property_get("sys.boot_completed", boot_completed) == 1 &&
            boot_completed[0] == '1') {
            Send("FRAMEWORK_BOOT_CONFIRMED watchdog released");
            _exit(0);
        }
        Stop("FAIL watchdog framework boot incomplete");
    }
    if (child < 0) Stop("FAIL watchdog fork");
}

inline void LoadWifi() {
    constexpr char path[] = "/vendor/lib/modules/klimt-wifi";
    android::base::unique_fd folder(open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    struct stat st {};
    if (!folder.ok() || fstat(folder.get(), &st) || st.st_uid != 0 || (st.st_mode & 022) != 0) {
        Send("WIFI_STAGE unavailable");
        return;
    }
    if (!android::base::WriteStringToFile("/vendor/firmware",
            "/sys/module/firmware_class/parameters/path")) {
        Send("WIFI_STAGE firmware path error " + std::to_string(errno));
        return;
    }
    auto load = [&](const char* name) {
        android::base::unique_fd module(openat(folder.get(), name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
        if (!module.ok() || fstat(module.get(), &st) || !S_ISREG(st.st_mode) ||
            st.st_uid != 0 || (st.st_mode & 022) != 0) {
            Send(std::string("WIFI_STAGE invalid ") + name);
            return false;
        }
        if (syscall(SYS_finit_module, module.get(), "", 0) && errno != EEXIST) {
            Send(std::string("WIFI_STAGE error ") + name + " " + std::to_string(errno));
            return false;
        }
        Send(std::string("WIFI_STAGE load ") + name);
        return true;
    };
    if (!load("cfg80211.ko") || !load("brcmutil.ko")) return;

    // The core can otherwise probe before the separate WCC provider is registered.
    constexpr char autoprobe[] = "/sys/bus/sdio/drivers_autoprobe";
    std::string state;
    if (!android::base::ReadFileToString(autoprobe, &state) ||
        android::base::Trim(state) != "1" ||
        !android::base::WriteStringToFile("0", autoprobe)) {
        Send("WIFI_STAGE cannot defer SDIO probe");
        return;
    }
    const bool loaded = load("brcmfmac.ko") && load("brcmfmac-wcc.ko");
    constexpr char bind[] = "/sys/bus/sdio/drivers/brcmfmac/bind";
    if (loaded) {
        for (int attempt = 0; attempt != 60 && access(bind, W_OK) != 0; ++attempt)
            usleep(50000);
    }
    if (!android::base::WriteStringToFile("1", autoprobe)) {
        Send("WIFI_STAGE cannot restore SDIO autoprobe");
        return;
    }
    if (!loaded) return;

    for (const char* address : {"mmc1:0001:1", "mmc1:0001:2"}) {
        const std::string radio = std::string("/sys/bus/sdio/devices/") + address;
        std::string vendor, device;
        if (!android::base::ReadFileToString(radio + "/vendor", &vendor) ||
            android::base::Trim(vendor) != "0x02d0" ||
            !android::base::ReadFileToString(radio + "/device", &device) ||
            android::base::Trim(device) != "0x4354") {
            Send(std::string("WIFI_STAGE BCM4354 identity unavailable ") + address);
            return;
        }
        char target[192] = {};
        const auto count = readlink((radio + "/driver").c_str(), target, sizeof(target) - 1);
        const bool bound = count > 0 &&
                android::base::EndsWith(std::string_view(target, count), "/brcmfmac");
        if (!bound && !android::base::WriteStringToFile(address, bind)) {
            Send(std::string("WIFI_STAGE SDIO bind error ") + address + " " + std::to_string(errno));
            return;
        }
        Send(std::string("WIFI_STAGE SDIO bound ") + address);
    }
    Send("WIFI_STAGE SDIO bound with WCC provider");
    for (int attempt = 0; attempt != 50 && access("/sys/class/net/wlan0", F_OK) != 0;
         ++attempt) usleep(100000);
    if (access("/sys/class/net/wlan0", F_OK) != 0) return;
    Send("WIFI_STAGE wlan0 registered");


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

inline bool ReadNodeNumber(const std::string& file, dev_t* device) {
    std::string data;
    unsigned int major_id, minor_id;
    if (!android::base::ReadFileToString(file, &data) ||
        sscanf(data.c_str(), "%u:%u", &major_id, &minor_id) != 2) return false;
    *device = makedev(major_id, minor_id);
    return true;
}

inline void MakePrivateNode(const char* name, const std::string& sys, int type) {
    dev_t number;
    struct stat st {};
    if (!ReadNodeNumber(sys, &number) ||
        (mknod(name, type | 0600, number) && errno != EEXIST) ||
        lstat(name, &st) || (st.st_mode & S_IFMT) != type || st.st_rdev != number) {
        Stop(std::string("FAIL temporary device node ") + name);
    }
}

inline void MountPersistentData() {
    const char* source = "/dev/klimt-userdata";
    const std::string device = "/sys/class/block/mmcblk0p21";
    for (int retry = 0; retry < 80 && access((device + "/dev").c_str(), R_OK); ++retry) {
        usleep(100000);
    }
    std::string uevent;
    if (!android::base::ReadFileToString(device + "/uevent", &uevent) ||
        uevent.find("\nPARTNAME=USERDATA\n") == std::string::npos ||
        uevent.find("\nDEVNAME=mmcblk0p21\n") == std::string::npos) {
        Stop("FAIL internal USERDATA identity");
    }
    MakePrivateNode(source, device + "/dev", S_IFBLK);
    android::base::unique_fd block(open(source, O_RDONLY | O_CLOEXEC));
    int ro = 1;
    uint64_t size = 0;
    if (!block.ok() || ioctl(block.get(), BLKROGET, &ro) || ro ||
        ioctl(block.get(), BLKGETSIZE64, &size) || size < 1024ULL * 1024 * 1024 ||
        mount(source, "/data", "ext4", MS_NOSUID | MS_NODEV | MS_NOATIME,
              "errors=remount-ro")) {
        Stop("FAIL persistent ext4 /data");
    }
    struct stat node {}, mounted {};
    struct statvfs options {};
    if (stat(source, &node) || stat("/data", &mounted) || mounted.st_dev != node.st_rdev ||
        statvfs("/data", &options) || (options.f_flag & ST_RDONLY)) {
        Stop("FAIL verified writable USERDATA mount");
    }
    Send("PERSISTENT_DATA_READY mmcblk0p21");
}

inline void CheckRoot(dev_t expected) {
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
    MountTrialCgroups();
    Send("SYSTEM_ROOT_VERIFIED exec installed selinux_setup");
}
}  // namespace klimt_trial
