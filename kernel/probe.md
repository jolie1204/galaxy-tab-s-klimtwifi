# SM-T700 Linux 6.12 headless boot probe

**The first four hardware boot attempts on November 19, 2026 are described below.** The [fifth trial](early-adb/README.md#sm-t700-hardware-trial) used a persistent RAM console and early ADB. This is a stand-alone Linux diagnostic for `klimtwifi`, with no Android userspace. The embedded init mounts procfs and sysfs and attempts to bring up USB Ethernet. It does not mount the tablet's eMMC partitions.

## Recovery and boot-loader constraints

- The tested Android 3.4.113 kernel has `CONFIG_KEXEC` disabled. In TWRP 3.7.0_9-0, `sys_kexec_load` resolves to the unimplemented syscall stub. Normal ADB recovery and return to the original Android work while the installed kernel responds.
- The Galaxy Tab board device tree documents a TLB invalidation needed for Samsung's stock S-BOOT. The [ARMv7 patch](armv7-sboot-tlb.patch) adapts Arve Hjønnevåg's proposal to the Linux 6.12 decompressor. In the built binary, the guarded invalidate follows the TTBR0 write and precedes the DACR write. [Board source](https://github.com/torvalds/linux/blob/v6.12/arch/arm/boot/dts/samsung/exynos5420-galaxy-tab-common.dtsi) · [Original patch submission](https://lists.openwall.net/linux-kernel/2012/12/12/55)
- The working boot image header declares `0x10008000`; TWRP reports kernel code beginning at `0x20008000` inside physical RAM. These describe different stages of boot. The S-BOOT entry address of the compressed image is unmeasured. The probe carries the existing format's kernel and tags fields and embeds initramfs to avoid an external ramdisk overlapping the expanded kernel. S-BOOT acceptance of a zero-size external ramdisk remains unconfirmed.
- The resulting Android boot v0 image was 6,068,224 bytes and fits the 8,388,608-byte BOOT partition. The host verified the appended DTB, initramfs, and decompressor instructions. TWRP received the exact image in RAM with a matching SHA-256. BOOT and RECOVERY matched their previous hashes, and the tablet returned to the existing Android installation.

Before a device boot attempt, confirm you can use the SM-T700 hardware keys to enter the existing TWRP or Download Mode and that you have a verified copy of the current device's complete BOOT partition. If the probe fails before USB initialization, USB cannot trigger the return to TWRP. The probe has no verified automatic rollback. [SM-T700 hardware recovery and Download Mode](https://github.com/LineageOS/lineage_wiki/blob/main/_data/devices/klimtwifi.yml)

## First hardware trial

TWRP verified the saved 8 MiB Android 14 BOOT against the live partition. The diagnostic file (6,068,224 bytes, SHA-256 `af7eea50c7054712cf38a4fba0143f0de77fba99e3ffccddb9ef63c02027b938`) replaced the BOOT prefix. After syncing and flushing the block device, the full partition matched the expected hash of the diagnostic plus the original tail. The separate RECOVERY partition also matched its pre-write hash. One normal reboot was requested.

At 10:12:58 PST the host requested the reboot. TWRP disconnected one second later. During 150 seconds of observation, the same physical USB port registered no new device or network interface. There was no interface on which to run the ICMP test. No UART trace was available. Kernel entry and embedded init execution remain unconfirmed. The owner later returned the tablet to TWRP. The captured previous-boot log contained neither a 6.12.118 version string nor the embedded init marker.

The known-booting Android 10, Android 14 and TWRP images each carry a 16-byte `SEANDROIDENFORCE` marker immediately after the page-aligned payload. The diagnostic file ends at that boundary, and its external ramdisk size is zero. The older marker in the preserved partition tail is at the Android 14 offset. The bootloader's handling of these differences is unknown.

### Android 14 restoration

TWRP 3.7.0_9-0 verified the full diagnostic BOOT and original RECOVERY hashes before restoring the saved 8,388,608-byte pre-trial Android 14 BOOT. After syncing and flushing the block device, TWRP read back the complete partition and matched it to the backup. RECOVERY retained its original hash. The restore wrote only BOOT.

LineageOS 21 Slim booted on Linux `3.4.113-Deathly-1.0+`. After Android first reported boot complete, `system_server` restarted once with an `OutOfMemoryError` while binding an account authenticator. After the restart the boot animation stopped, setup was complete, Bluetooth reported ON, the default Wi-Fi network was validated, and Package Manager found Play Store, Play services and Wemo Controller 4.4.0. The error’s underlying cause remains unknown.

## Second hardware trial

On November 19, 2026, a second boot image reused the first probe's exact compressed kernel, appended DTB and embedded init. The container added a gzip copy of the same initramfs as an external ramdisk and the 16-byte `SEANDROIDENFORCE` marker at the aligned payload end. The external ramdisk header offset was 64 MiB from the base. The expanded kernel image was 22,596,756 bytes, larger than the working Android header's 16 MiB ramdisk offset. Actual S-BOOT load addresses remain unmeasured.

To produce this variant after the build below, keep the same kernel, command line and mkbootimg arguments. Create the external archive with `gzip -n -c "$kernel_out/usr/initramfs_data.cpio" > "$kernel_out/initramfs.cpio.gz"`, add `--ramdisk "$kernel_out/initramfs.cpio.gz" --ramdisk_offset 0x04000000` to mkbootimg, and select a separate output file. Append the marker to the newly generated image with `printf %s SEANDROIDENFORCE >> "$image"`, where `image` is that output path. Unpack it and compare both payloads to their inputs. Verify that the marker immediately follows the aligned ramdisk.

The hardware candidate was 6,211,600 bytes (SHA-256 `d6541d0a85a564aa0c427209c608726d2b0f01c5b7e541fd7c0a7efc96d30f7d`). TWRP verified the original Android BOOT and separate RECOVERY. After writing the candidate, flushing and reading the entire 8 MiB BOOT, its hash matched the expected candidate plus preserved tail. RECOVERY still matched its original hash.

The host requested a normal reboot at 13:10:14 PST. It monitored the tablet's physical USB port before reboot through 13:13:19 PST. TWRP disconnected after three seconds. The port stayed empty for the remaining 182 seconds, and the host kernel journal contained only the disconnect. There was no USB interface for the ping test. No UART or screen observation was available. Linux 6.12.118 kernel entry and embedded init execution remain unconfirmed. The owner subsequently reported a downloader screen and later returned the device to TWRP.

### Recovery after the second trial

TWRP 3.7.0_9-0 exposed a 6,859-byte previous-boot log containing recovery init messages. It contained neither the Linux 6.12.118 version nor the diagnostic init marker. TWRP verified the full second-trial BOOT hash, then restored the 8 MiB Android 14 BOOT backup. The complete post-flush read-back matched the backup, and the separate RECOVERY partition retained its original hash.

Android 14 Slim reported boot complete at 19:29:39 PST on Linux `3.4.113-Deathly-1.0+`. By 19:30:08 PST Android reported a validated default Wi-Fi connection, Bluetooth ON and setup complete. Package Manager found Play Store, Play services and Wemo Controller 4.4.0. The same system_server process was present two minutes after boot completion, and the crash buffer contained no fatal system-process exception at that check.

## Third hardware trial with a microSD record

This variant retains the USB diagnostic and adds a separate way to prove that Linux 6.12.118 reached userspace. Android created one fresh `K612-<run-id>` directory on the existing public FAT volume. Its `run-id` file contains a random 32-character hex string followed by a newline. The SD writer accepts only an MMC device whose sysfs type is `SD` and whose partition 1 start and length match the prepared card in 512-byte sectors. It mounts that partition read-only first and checks the run ID before a writable remount. It creates its output exclusively, writes the actual uname, fsyncs and unmounts. It never selects an MMC device whose type is `MMC`.

For this variant, generate a private `sd_run.h` with quoted string values for `RUN_ID`, `PART_START` and `PART_SECTORS`. Read the sectors from the verified external partition's sysfs `start` and `size`. Build [sd_probe.c](sd_probe.c) with the same static ARM compiler options as the USB init below and add the private header directory with `-I`. In the initramfs list install the SD executable at `/init`, the original USB init at `/usb-init`, and add `dir /tmp 0700 0 0`. Build both the embedded and external initramfs from that list and package as in the second trial.

Before writing BOOT, TWRP ran the same SD executable with `--control` while the card was unmounted. It created `control.txt` with the run ID and TWRP's actual kernel release. A read-only remount verified the file. A second control invocation refused to overwrite it, and a mismatched run was rejected. `result.txt` was absent before reboot. The live diagnostic can create that file only when its kernel reports `6.12.118`. A complete matching result establishes userspace execution. An absent result leaves both early boot and SD initialization open.

The 6,498,320-byte container passed payload comparisons and full BOOT read-back. Its SHA-256 was `f186943352029ee9ef55543426958e91b84ed8ce195a980e39960fab5e27e591`. RECOVERY matched its original whole-partition hash. One normal boot was requested at 20:13:58 PST. USB disconnected two seconds into the 185-second observation. No new USB device appeared during the remaining 183 seconds. At 20:17:33 PST, TWRP mounted the card read-only. Both the run ID and control record matched their host copies byte for byte. The run directory contained no `result.txt`. Recovery's previous-boot log included recovery init messages and contained no 6.12.118 version or probe marker. This trial produced no evidence of Linux 6.12.118 reaching the SD writer. The observations leave kernel entry, early init failure and SD driver or filesystem readiness unresolved.

TWRP restored the pre-trial Android 14 BOOT. The complete 8 MiB post-flush read-back matched the backup, and RECOVERY retained its original hash. Android also read the matching control and confirmed that the SD result was absent. The restored Android 14 system reported the 3.4.113 Deathly kernel. During startup the Android framework restarted twice with system_server memory errors, once while parceling package information and once while reporting Wi-Fi scan results. At 20:20:23 PST the replacement process was running with setup complete, a validated Wi-Fi connection, Bluetooth ON, and Play Store, Play services and Wemo Controller 4.4.0 installed. The underlying cause remains unresolved.

## Fourth trial with USB CDC ACM

On a separate SM-T280, a Spreadtrum SC7730-family Linux 7.3.1 RAM diagnostic first enumerated as a USB CDC ACM gadget but produced no text. After the initramfs mounted devtmpfs on `/dev` with device nodes usable, the host received the kernel release and a read-only eMMC report. That project then tested SMP and peripherals individually before moving to Android services. Its Android 14 work reached PackageManager and graphics milestones. The stock ADB switch still had a missing userspace dependency at its last recorded checkpoint.

This SM-T700 experiment adopts the two-stage USB measurement over a standard data cable. It keeps Linux 6.12.118, the Exynos platform code, the SM-T700 DT and the S-BOOT TLB change. The running vendor kernel identified `exynos-ss-udc.0` under `exynos-dwc3.0` as its USB device controller. The upstream board describes two peripheral DWC3 instances. A trial-only [DT overlay](klimt-acm-port.dts) disables instance 1 to avoid binding the gadget there. This selection remains a hardware hypothesis.

The build uses `CONFIG_USB_G_SERIAL=y` in its default CDC ACM mode, disables `CONFIG_USB_ETH` and forces `maxcpus=1 rdinit=/init ro loglevel=7` for this diagnostic. The [RAM init](acm_probe.c) mounts devtmpfs without `MS_NODEV` and mounts sysfs. Every two seconds it attempts to send its marker, the running kernel release, the machine and online CPU list on `/dev/ttyGS0`. The diagnostic mounts no eMMC or SD filesystem.

### Reproducing the candidate

Start from the patched source and isolated output tree in **Build locally** below. Compile `acm_probe.c` in place of `probe_init.c` with the same static ARM compiler options. Point the initramfs list's `file /init` at that executable. Before `olddefconfig` and the kernel build, apply these settings:

```bash
boot_args='maxcpus=1 rdinit=/init ro loglevel=7'
"$kernel_src/scripts/config" --file "$kernel_out/.config" \
  --disable USB_ETH --enable USB_G_SERIAL \
  --disable CMDLINE_FROM_BOOTLOADER --enable CMDLINE_FORCE \
  --set-str CMDLINE "$boot_args" --disable LOCALVERSION_AUTO \
  --set-str LOCALVERSION '-klimt-acm1'
```

After the kernel and board DTB build, apply the overlay before appending the DTB:

```bash
"$kernel_out/scripts/dtc/dtc" -I dts -O dtb -o "$kernel_out/acm.dtbo" klimt-acm-port.dts
"$kernel_out/scripts/dtc/fdtoverlay" \
  -i "$kernel_out/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dtb" \
  -o "$kernel_out/klimt-acm.dtb" "$kernel_out/acm.dtbo"
cat "$kernel_out/arch/arm/boot/zImage" "$kernel_out/klimt-acm.dtb" > "$kernel_out/zImage-klimtwifi-probe-dtb"
```

Use the **Second hardware trial** container procedure with the new kernel, command line and initramfs, the 64 MiB ramdisk offset and the aligned Samsung trailer. The tested file was 6,209,552 bytes with SHA-256 `51e74ddf1dcbc176dac11105ac77634673146ad8ad01ae8076857a48754525cd`. Host checks compared the unpacked payloads, extracted init binary, compressed embedded archive, selected controller and expected kernel release.

Start capturing the target's physical USB port before reboot. A new `0525:a4a7` device on that port is the first checkpoint. Open its own newly attached `ttyACM` node. The second checkpoint requires the actual `SM-T700 ACM RAM INIT READY` report and `release=6.12.118-klimt-acm1`. Enumeration alone cannot establish PID 1 execution.

The SM-T700 trial rebooted at 20:34:42 PST on November 19. The monitor started in TWRP and ran for 185 seconds. The device disconnected after three seconds and the same port registered no new device for the remaining 182 seconds. The host journal also showed only the disconnect. No `ttyACM` appeared on the target port and zero report bytes were received. Neither a working USB gadget nor execution of the RAM init was demonstrated. These observations do not locate the failure relative to kernel entry.

TWRP returned at 20:38:33 PST. Its previous-boot log contained recovery init messages and no ACM marker or 6.12.118 release. Before rollback the complete BOOT matched the fourth candidate plus preserved tail. The complete flushed BOOT matched the original Android 14 backup after restoration, and RECOVERY retained its original hash. Android's running vendor configuration disables standalone `CONFIG_USB_G_SERIAL` and enables Samsung's composite gadget. Vendor ADB verifies the cable and the vendor USB path. Mainline DWC3/ACM remains unverified. The restored Android 14 reached boot completion on Linux 3.4.113. For subsequent experiments on this unit the owner chose to keep diagnostic BOOT between trials, collect evidence in TWRP, and defer Android restoration until Linux bring-up succeeds.

## Build locally

Use the source archive, NDK r28b and checked configuration specified in [the kernel build notes](README.md). Start with a fresh extraction of Linux 6.12.118. Run these commands from this directory, with `NDK_ROOT` set and the standard Linux kernel build tools installed.

```bash
kernel_src="$PWD/linux-6.12.118"
kernel_out="$PWD/out/probe"
toolchain="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin"
mkdir -p "$kernel_out"

patch --fuzz=0 -p1 -d "$kernel_src" < armv7-sboot-tlb.patch
"$toolchain/armv7a-linux-androideabi24-clang" -Os -static -Wall -Wextra -Werror \
  probe_init.c -o "$kernel_out/probe-init"
"$toolchain/llvm-strip" --strip-all "$kernel_out/probe-init"
cp exynos_xz.config "$kernel_out/.config"
cat > "$kernel_out/initramfs.list" <<EOF
dir /dev 0755 0 0
dir /proc 0555 0 0
dir /sys 0555 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/null 0666 0 0 c 1 3
file /init $kernel_out/probe-init 0755 0 0
EOF
boot_args='console=ttySAC2,115200n8 rdinit=/init ro loglevel=7'
"$kernel_src/scripts/config" --file "$kernel_out/.config" --set-str CMDLINE "$boot_args"
"$kernel_src/scripts/config" --file "$kernel_out/.config" \
  --set-str INITRAMFS_SOURCE "$kernel_out/initramfs.list"
make -C "$kernel_src" O="$kernel_out" ARCH=arm LLVM="$toolchain/" olddefconfig
make -C "$kernel_src" O="$kernel_out" ARCH=arm LLVM="$toolchain/" \
  -j"$(nproc)" zImage samsung/exynos5420-klimt-wifi.dtb
cat "$kernel_out/arch/arm/boot/zImage" \
  "$kernel_out/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dtb" \
  > "$kernel_out/zImage-klimtwifi-probe-dtb"
```

Package the appended image with the AOSP tool pinned to Android 14. The stock header supplies the address fields below. This step creates a candidate boot image for offline inspection.

```bash
git clone --depth 1 --branch android-14.0.0_r1 \
  https://android.googlesource.com/platform/system/tools/mkbootimg "$kernel_out/mkbootimg"
python3 "$kernel_out/mkbootimg/mkbootimg.py" --header_version 0 --pagesize 2048 \
  --kernel "$kernel_out/zImage-klimtwifi-probe-dtb" \
  --base 0x10000000 --kernel_offset 0x00008000 --tags_offset 0x00000100 \
  --cmdline "$boot_args" --output "$kernel_out/linux-6.12.118-klimtwifi-probe.img"
python3 "$kernel_out/mkbootimg/unpack_bootimg.py" \
  --boot_img "$kernel_out/linux-6.12.118-klimtwifi-probe.img" --out "$kernel_out/unpacked"
cmp "$kernel_out/unpacked/kernel" "$kernel_out/zImage-klimtwifi-probe-dtb"
test "$(stat -c %s "$kernel_out/linux-6.12.118-klimtwifi-probe.img")" -le 8388608
sha256sum "$kernel_out/linux-6.12.118-klimtwifi-probe.img"
```

A device trial can look for a new USB Ethernet interface on the host. If it enumerates, use `169.254.70.1/30` on that specific interface and ping `169.254.70.2`. An ICMP reply would show that the embedded init configured `usb0`. The test's serial console is `ttySAC2` at 115200 baud. Some failures would leave both interfaces unavailable. Display output, Wi-Fi and Android integration are outside this probe.
