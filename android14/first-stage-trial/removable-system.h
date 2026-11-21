// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <linux/loop.h>
#include <sys/mount.h>

#include "mount-trial.h"

namespace klimt_trial {

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

inline dev_t MountRemovableSystem() {
    const char* media = "/dev/klimt-sd";
    const char* sd_node = "/dev/klimt-sd-part";
    const char* control_node = "/dev/klimt-loop-control";
    const char* loop_node = "/dev/klimt-system-loop";
    std::string card_type;
    dev_t sd_number;
    for (int retry = 0; retry < 80; ++retry) {
        if (ReadNodeNumber("/sys/class/block/mmcblk2p1/dev", &sd_number)) break;
        usleep(100000);
    }
    if (!android::base::ReadFileToString("/sys/class/block/mmcblk2/device/type", &card_type) ||
        android::base::Trim(card_type) != "SD") Stop("FAIL removable SD identity");
    MakePrivateNode(sd_node, "/sys/class/block/mmcblk2p1/dev", S_IFBLK);
    android::base::unique_fd sd(open(sd_node, O_RDONLY | O_CLOEXEC));
    int sd_ro = 0;
    if (!sd.ok() || ioctl(sd.get(), BLKROGET, &sd_ro) || !sd_ro ||
        (mkdir(media, 0700) && errno != EEXIST) ||
        mount(sd_node, media, "vfat", MS_RDONLY | MS_NODEV | MS_NOSUID | MS_NOEXEC,
              "iocharset=iso8859-1")) Stop("FAIL read-only removable FAT mount");

    android::base::unique_fd backing(open("/dev/klimt-sd/KLIMT61246/SYSTEM2.EXT",
                                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    struct stat64 info {};
    uint8_t magic[2] = {};
    if (!backing.ok() || fstat64(backing.get(), &info) || !S_ISREG(info.st_mode) ||
        info.st_size != 2527068160ULL || pread(backing.get(), magic, 2, 1080) != 2 ||
        magic[0] != 0x53 || magic[1] != 0xef) Stop("FAIL staged ext4 image identity");

    MakePrivateNode(control_node, "/sys/class/misc/loop-control/dev", S_IFCHR);
    android::base::unique_fd control(open(control_node, O_RDWR | O_CLOEXEC));
    const int loop_id = control.ok() ? ioctl(control.get(), LOOP_CTL_GET_FREE) : -1;
    if (loop_id < 0) Stop("FAIL loop allocation");
    MakePrivateNode(loop_node, "/sys/class/block/loop" + std::to_string(loop_id) + "/dev", S_IFBLK);
    android::base::unique_fd loop(open(loop_node, O_RDONLY | O_CLOEXEC));
    loop_config cfg {};
    cfg.fd = backing.get();
    cfg.block_size = 4096;
    cfg.info.lo_flags = LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR;
    int loop_ro = 0;
    if (!loop.ok() || ioctl(loop.get(), LOOP_CONFIGURE, &cfg) ||
        ioctl(loop.get(), BLKROGET, &loop_ro) || !loop_ro || fstat64(loop.get(), &info) ||
        (mkdir("/system", 0755) && errno != EEXIST) ||
        mount(loop_node, "/system", "ext4", MS_RDONLY | MS_NOATIME, "noload")) {
        Stop("FAIL read-only removable Android system");
    }
    if (umount2(media, MNT_DETACH)) Stop("FAIL detach private FAT mount");
    Send("SD_SYSTEM_RO ext4 via loop" + std::to_string(loop_id));
    return info.st_rdev;
}

}  // namespace klimt_trial
