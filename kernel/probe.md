# SM-T700 Linux 6.12 headless boot probe

**Two hardware boot attempts on November 19, 2026.** This is a stand-alone Linux diagnostic for `klimtwifi`, with no Android userspace. The embedded init mounts procfs and sysfs and attempts to bring up USB Ethernet. It does not mount the tablet's eMMC partitions.

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

The host requested a normal reboot at 13:10:14 PST. It monitored the tablet's physical USB port before reboot through 13:13:19 PST. TWRP disconnected after three seconds. The port stayed empty for the remaining 182 seconds, and the host kernel journal contained only the disconnect. There was no USB interface for the ping test. No UART or screen observation was available. Linux 6.12.118 kernel entry and embedded init execution remain unconfirmed. At the close of this observation the second probe occupied BOOT, pending physical return to TWRP for Android restoration.

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
