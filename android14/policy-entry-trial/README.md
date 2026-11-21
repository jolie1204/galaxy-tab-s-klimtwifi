# Android 14 SELinux and second-stage entry on SM-T700

Trial 21 uses the original dynamic init and policy from the installed Android SYSTEM. The static first-stage init runs from a diagnostic ramdisk. The kernel observes the policy handed to SELinux and PID 1's credentials at second-stage entry. The tablet returned to TWRP before the Android property service started. [Hardware result and timings](../../docs/android14-source.md#selinux-and-second-stage-init-on-linux-612-trial-21)

## Building the diagnostic

1. Start with the trial 20 kernel configuration, board DT, decompressor wrapper, single-row SYSTEM fstab and static first-stage init patch. Apply `continue-after-root.patch` next to `mount-trial.h` before rebuilding the static init. This keeps the ext4, block device, mount flag and executable checks and permits the normal handoff to the installed init. Put a fresh run ID in the ramdisk and use the same ID in the decompressor records. [Trial 20 inputs](../first-stage-trial/README.md)
2. Apply `linux-6.12.118.patch` from an isolated Linux 6.12.118 source tree with `patch --fuzz=0 -p1`. Place `policy-entry.h` in `security/selinux/`. Build with SELinux, the SHA-256 library, the MMC block driver, USB gadget serial, its console, and the diagnostic ramdisk built in. All MMC disks are registered read-only for this kernel boot.
3. Enable `CONFIG_BOOT_CONFIG`, `CONFIG_BOOT_CONFIG_EMBED` and `CONFIG_BOOT_CONFIG_FORCE`. Point `CONFIG_BOOT_CONFIG_EMBED_FILE` at the supplied `trial.bootconfig`. The device's forced command line is shown below. Keep the fatal reboot target in bootconfig so the installed Android 14 binary reads it there.

```text
maxcpus=1 rdinit=/init ro loglevel=7 printk.devkmsg=on console=ttyGS0 g_serial.iSerialNumber=SMT700-POLICY androidboot.hardware=universal5420 androidboot.boot_devices=soc/12200000.mmc androidboot.init_rc=/dev/null
```

The USB observer selected the original tablet port, CDC ACM ID `0525:a4a7` and serial `SMT700-POLICY`. It opened the associated `ttyACM*` with DTR asserted before collecting the live log. The embedded bootconfig is retrievable inside Linux at `/proc/bootconfig`.

## Interpreting the evidence

After SELinux accepts a policy from PID 1, the diagnostic hashes the kernel's copy of the supplied bytes. On the exact native second-stage log message from PID 1, a task-work callback checks the committed policy flag, global enforcing mode and the current SELinux context. This callback runs before returning to userspace, after the log write released its locks. The callback parks PID 1 and reschedules a dedicated recovery worker for six seconds later. A separate 55-second deadline starts during kernel initialization. The empty Android boot-script selection and read-only MMC devices also apply if the expected log message is missing.

The host's positive ARM QEMU control booted a read-only snapshot of the tablet's SYSTEM. The kernel reported a policy digest matching the file, the native second-stage entry, enforcing mode and `u:r:init:s0`. The negative control used a separate snapshot with four bytes changed in the policy magic. SELinux rejected it before committing a policy, and Android requested recovery. A third QEMU control suppressed userspace kmsg writes. The kernel emitted its policy commit marker, omitted the entry verdict and requested recovery on its independent timeout at uptime 59.39 s. Three replays of the final Samsung-wrapped BOOT independently matched the expanded kernel image and ramdisk.

## ARM32 fatal-reboot parameter

The initial negative control exposed a bug in the installed Android command-line parser. It advanced by `sizeof(std::string) - 1` when it needed the length of the parameter name and equals sign. On ARM32 the resulting restart command was `.init_fatal_reboot_target=recovery`. Supplying the parameter through embedded bootconfig yielded the exact `recovery` command in both host controls. The physical success path also returned to TWRP.

For a future Android SYSTEM rebuild, `android-init-fatal-target.patch` corrects the original parser in `system/core/init/reboot_utils.cpp` at `system/core` revision `cf103cae1bd46f6be19d1e407a6ea55baa38cb83`. The changed function passed host cases for a command-line parameter at the beginning, middle and end, bootconfig fallback and an explicit override. Trial 21 ran the installed init binary with the bootconfig workaround.
