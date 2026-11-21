# Android 14 property service and cgroups on SM-T700

This diagnostic lets the installed Android 14 init run through property service startup and the cgroup setup action. The selected init script contains a single early-init action with no imports or service declarations. The boot retains the trial 21 SELinux policy checks, read-only MMC disks and independent recovery timer. [Trial 21 inputs](../policy-entry-trial/README.md)

## Build inputs

1. Start with the trial 21 first-stage ramdisk and Linux 6.12.118 sources. Apply `first-stage.patch` to its `mount-trial.h`, add `property-ramdisk.h` alongside it, and rebuild static `init_first_stage`. Use `property-entry.h` in place of trial 21's `security/selinux/policy-entry.h`. Merge `cgroups.config` into the diagnostic kernel configuration and verify the options after `olddefconfig`.
2. Read `/system/etc/cgroups.json` from the installed SYSTEM. Run `python3 prepare-cgroups.py stock.json trial.json`. Place the original and generated files in the ramdisk as `/klimt-cgroups.original` and `/klimt-cgroups.json`. Include `/klimt-property.rc` and the fresh 16-character hexadecimal `/klimt-trial-id`. These files are root-owned, mode 0444. After switching root, the diagnostic verifies the installed cgroup file byte for byte against its original and bind-mounts the RAM copy read-only over it.
3. Keep the recovery target in embedded Android bootconfig. Set `androidboot.klimt_trial` there to the same fresh ID. Use `androidboot.init_rc=/dev/klimt-property.rc` in the kernel command line and `g_serial.iSerialNumber=SMT700-PROP` for the ACM observer. The probe script is copied into the RAM-backed `/dev` before switching root.

## What the probe measures

The stock product reports SDK 34 and first API level 19. Its default cgroup table contains an optional `schedtune` row. The trial removes this one row while preserving the other controller definitions, ownership and mount paths. Utilization clamping is enabled on the CPU controller. Task migration and tuning under application load are separate measurements. [Android cgroup and task-profile documentation](https://source.android.com/docs/core/perf/cgroups)

The client is Android init's `setprop` builtin. In this source revision it calls `android::base::SetProperty`, which invokes Bionic's property socket client. The first write sets a volatile, write-once property to the boot's fresh ID. The second requests a different value. The host checks the server's rejection with its peer PID, the original value read back from the property area, and the advertised service protocol. This exercises a socket request from PID 1 to its property-service thread.

The kernel inspects each controller from PID 1's mount namespace and verifies the filesystem magic of its control file. The checks cover block I/O, CPU shares, utilization clamping in a CPU child group, cpuset, memory and freezing in a cgroup2 child. The return-to-userspace callback also checks the loaded policy, enforcing mode and init domain, then requests recovery through the dedicated worker.
