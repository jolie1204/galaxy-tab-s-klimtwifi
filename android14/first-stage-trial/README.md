# SM-T700 Android first-stage mount diagnostic

Trial 20 boots the product's static Android 14 `init_first_stage` on Linux 6.12.118. It verifies Android's root switch to the existing internal SYSTEM and requests a software recovery reboot before executing the installed init. [Hardware results](../../docs/android14-source.md#android-14-first-stage-on-linux-612-trial-20)

## Inputs

1. In an isolated Android checkout, use the `lineage_klimtwifi-ap2a-userdebug` product. Apply `native-init.patch` with `patch --fuzz=0 -p1` from `system/core` (tested revision `cf103cae1bd46f6be19d1e407a6ea55baa38cb83`). Place `mount-trial.h` beside `init/first_stage_init.cpp`. Build the product's ARM32 static `init_first_stage`. The trial recompiled that translation unit and relinked with the product's existing Soong inputs.
2. In a copy of the product ramdisk, replace `/init` with that executable and `/fstab.universal5420` with the supplied single-row fstab. Add a fresh 16-character hexadecimal ID in `/klimt-trial-id`. Preserve the archive's file modes, ownership and ordering. Embed this archive in the inner 6.12 kernel.
3. Use the trial 19 board USB setting and Samsung decompressor handoff. In the board DT, supply `hardware = "universal5420"` and `boot_devices = "soc/12200000.mmc"` under `/firmware/android` with compatible `android,firmware`. Retain the Samsung secure firmware node. The diagnostic kernel includes `CONFIG_USB_G_SERIAL=y`, `CONFIG_U_SERIAL_CONSOLE=y`, `CONFIG_SECURITY_SELINUX=y`, `CONFIG_EXT4_FS_SECURITY=y`, `CONFIG_PSTORE_CONSOLE=y` and `CONFIG_PSTORE_RAM=y`. [USB board setting](../../kernel/early-adb/usb2-lpm-off.dts) · [Samsung decompressor handoff](../../kernel/decompressor/vendor-control/)

The tested diagnostic used this built-in command line with `CONFIG_CMDLINE_FORCE=y`.

```text
maxcpus=1 rdinit=/init ro loglevel=7 console=ttyGS0 g_serial.iSerialNumber=SMT700-FIRSTSTAGE androidboot.hardware=universal5420 androidboot.boot_devices=soc/12200000.mmc
```

## Reading the result

Monitor the tablet's physical USB port before reboot. When the Linux CDC ACM device enumerates (`0525:a4a7`, serial `SMT700-FIRSTSTAGE`), open its corresponding host `ttyACM*` for raw input with DTR asserted. The tty number can change. The kernel supplies its console on `ttyGS0`, and PID 1 repeats its verdict directly on the gadget serial port. On return to TWRP, read `/proc/last_kmsg` to corroborate the run ID and recovery request independently.

The diagnostic requires all of the following to emit `PASS SYSTEM_ROOT`.

- Native first-stage device discovery creates the by-name link, and its block device identifies itself as `SYSTEM` and `mmcblk0p18`.
- The kernel confirms the block device is read-only before Android attempts the ext4 mount with `ro,noload`.
- After Android's root switch, `/` is a read-only ext4 mount on that same device and `/system/bin/init` is an executable little-endian ARM32 ELF on it.

The positive ARM QEMU control used the same Android ramdisk, a virtual SD controller and an ext4 SYSTEM on GPT partition 18. The negative control omitted the card. Android timed out waiting for SYSTEM and requested recovery without a success verdict. Three separate Cortex-A15/A7 replays of the packaged tablet BOOT matched the expanded 6.12 image. The tablet trial compared the run ID and verdict across live USB and recovery's previous-console reads.
