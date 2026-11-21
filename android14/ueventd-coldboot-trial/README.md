# Android 14 ueventd coldboot on SM-T700

Trial 23 runs the installed `/system/bin/ueventd`, a symlink to `init`, as the only service declared in the diagnostic init script. Android's own coldboot routine enumerates devices, waits for its workers and publishes `ro.cold_boot_done`. The probe checks device creation in init's mount namespace and reads the property after init has passed its built-in coldboot wait. [Android 14 ueventd reference](https://android.googlesource.com/platform/system/core/+/refs/heads/android14-release/init/README.ueventd.md)

## Reproducing the diagnostic

1. Start with the trial 22 static first-stage init, cgroup configuration, read-only MMC guard and embedded bootconfig. Use a fresh run ID for the ramdisk, bootconfig and decompressor records. The preserved first-stage binary expects the diagnostic script at `/klimt-property.rc`. Store this trial's `klimt-coldboot.rc` under that name, root-owned and mode 0444, so first stage stages it at `/dev/klimt-property.rc`. [Trial 22 build inputs](../property-service-trial/README.md)
2. Use `coldboot-entry.h` in place of the earlier diagnostic header at `security/selinux/policy-entry.h`. Retain the kernel hook that records the policy digest. The physical command line selects `androidboot.init_rc=/dev/klimt-property.rc`, `androidboot.klimt_coldboot=1` and `g_serial.iSerialNumber=SMT700-COLD`. The timer independently requests recovery if init stalls.
3. Capture ACM output on the tablet's physical port. On recovery, compare the run ID, daemon PID, before/after node checks, Android's coldboot timing, final property and restart command with the retained kernel console.

The early-init action checks that the property and selected nodes are absent before starting the daemon. The service uses the installed executable, root UID and stock `u:r:ueventd:s0` label. The diagnostic uses explicit start and a one-shot service to avoid repeated launches. Its `on init` action reads both `ro.cold_boot_done` and `init.svc.ueventd` after Android's built-in wait.

The node checks require a character device on tmpfs with the expected mode, UID and GID. Looking up the device by major/minor under `/sys/dev/char` must resolve to the same sysfs object as its class entry. The tablet adds an input event and RTC to the full/zero checks shared with ARM QEMU. No node is opened for device I/O.

The negative QEMU control changes only `androidboot.klimt_coldboot` to `0`. Both controls run the same kernel, RAM script and installed SYSTEM snapshot. With the daemon start omitted, genuine coldboot completion and the final init action must stay absent until the timeout requests recovery.
