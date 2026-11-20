# Linux 6.12.118 SM-T700 bring-up experiment

**The headless diagnostic reached its first hardware boot attempt on November 19, 2026.** The first outputs were an ARM `zImage` and the mainline SM-T700 device tree. A later [headless boot probe](probe.md) packages an updated kernel with a small embedded initramfs. Android ramdisk and HAL integration remain separate work.

The test used Linux 6.12.118, the Android NDK r28b LLVM toolchain and the archived [configuration](exynos_xz.config) derived from upstream `exynos_defconfig`. It enables XZ kernel compression, appended DTB and the ARM ATAG/DTB compatibility option. [Kernel source archive](https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.118.tar.xz) · [Android NDK r28b](https://github.com/android/ndk/releases/tag/r28b)

From this directory, save the tarball locally and run the following with the NDK extracted and Linux kernel build tools available, including GNU make, flex, bison and the xz tools. Set `NDK_ROOT` to the extracted NDK path.

```bash
sha256sum -c source.sha256
tar -xf linux-6.12.118.tar.xz
kernel_src="$PWD/linux-6.12.118"
kernel_out="$PWD/out"
toolchain="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin"

mkdir -p "$kernel_out"
cp exynos_xz.config "$kernel_out/.config"
make -C "$kernel_src" O="$kernel_out" ARCH=arm LLVM="$toolchain/" olddefconfig
make -C "$kernel_src" O="$kernel_out" ARCH=arm LLVM="$toolchain/" \
  -j"$(nproc)" zImage samsung/exynos5420-klimt-wifi.dtb
cat "$kernel_out/arch/arm/boot/zImage" \
  "$kernel_out/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dtb" \
  > "$kernel_out/zImage-klimtwifi-dtb"
```

The archived build's appended image was 5,920,125 bytes, within the SM-T700 BOOT partition's 8,388,608 bytes. That comparison covers the kernel and DTB. The complete boot image still needs a size and load-address check. The [archived build checksums](archived-build.sha256) identify the historical outputs and configuration. Local build metadata can affect reproducibility.

The mainline board tree describes eMMC, SDIO Wi-Fi power sequencing and USB gadget support. Display panel support for the S6E3HA1, the Synaptics touch binding, Wi-Fi firmware/calibration, and Android binder and HAL integration remain open. The current configuration has binder and RMI4 core disabled. Keep the existing verified Android 14 BOOT image for normal use.

The subsequent [early-ADB candidate](early-adb/README.md) preserves the kernel console for recovery readback and starts authenticated ADB from RAM. It passed ARM emulation and reached a fifth SM-T700 hardware boot attempt on November 19.

The [sixth hardware trial](decompressor/README.md) places two checkpoints in the ARM decompressor, before the USB and pstore drivers initialize.

## Known-working Android 14 kernel on SM-T700

The October 7 LineageOS 21 ROM and its Slim derivative contain a byte-identical `boot.img` (SHA-256 `5dbe57bb2975236c084af24ff1344e32cc8fb5e8d82faf046a2318fcfed5081d`). The kernel reports `3.4.113-Deathly-1.0+`, built with ARM GCC `4.9.x 20150123 (prerelease)`. The maintainer's manifest selects the `lineage-21.0` Exynos5420 kernel branch, and the device selects `lineageos_klimtwifi_defconfig`. [Manifest](https://github.com/retiredtab/LineageOS-build-manifests/blob/b74363082f21b2d4a2493e0b36d25d0ae78345e5/21/exynos5420/5420-21.xml) · [Device BoardConfig](https://github.com/retiredtab/android_device_samsung_klimtwifi/blob/8dbcf8aba9efe773ed2230819dddc843aa19c71c/BoardConfig.mk) · [Common BoardConfig](https://github.com/retiredtab/android_device_samsung_universal5420-common/blob/b8f7784abffb34bc1c9639ade1603344d37f3bb3/BoardConfigCommon.mk)

On November 19, a host rebuild from `ece5708530bb` generated the configuration with `make ARCH=arm lineageos_klimtwifi_defconfig`, then built the `zImage` target with the ARM Android GCC 4.9 toolchain. The generated configuration matched both the kernel's embedded configuration and the saved running tablet configuration after decompression (SHA-256 `6b5c7abf34afbb417f6a01601bbe7c1434aca08b6d6a2238876bd52a81f5f82f`). The manifest names a branch and the original kernel banner omits a commit ID. [Kernel configuration](https://github.com/retiredcrave/android_kernel_samsung_exynos5420/blob/ece5708530bb80a85b26edeee568733b14740239/arch/arm/configs/lineageos_klimtwifi_defconfig) · [Toolchain revision used for the rebuild](https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9/tree/111258a10e017f067b27e6cfcea7619d753f3309)

| Artifact | October ROM | Host rebuild |
| --- | --- | --- |
| Uncompressed ARM Image | 14,987,772 bytes | 14,987,772 bytes |
| LZMA-compressed ARM zImage | 5,029,280 bytes | 5,028,296 bytes |
| zImage SHA-256 | `24a7478b7ce7b79eb58b10a2f1221a9f1c41495b967f4ae26e650b8496db606e` | `f096d3243949fd8c05943b8ad0b0aa3c1fa264c43a8ba7f6764ff634562c3dd6` |

The uncompressed images also differ. Visible differences include two driver build timestamps and a source path. The original contains a December 2024 Wi-Fi driver build string despite its October 2026 kernel banner. The rebuild ran only on the host.

The original BOOT header carries a kernel load address of `0x10008000`. At offset `0x60` in the ROM's zImage, the ARM decompressor loads register r4 from the literal at offset `0x7ec`, whose value is `0x20008000`. The same value appears as `zreladdr` in the rebuilt decompressor. The vendor kernel uses a compiled-in board definition, ATAGs and a fixed decompression destination. The 6.12 diagnostic uses an appended DTB and automatic destination selection. The physical address where S-BOOT enters the compressed image remains unmeasured. [Vendor boot addresses](https://github.com/retiredcrave/android_kernel_samsung_exynos5420/blob/ece5708530bb80a85b26edeee568733b14740239/arch/arm/mach-exynos/Makefile.boot) · [Vendor board file](https://github.com/retiredcrave/android_kernel_samsung_exynos5420/blob/ece5708530bb80a85b26edeee568733b14740239/arch/arm/mach-exynos/mach-universal5420.c)

## Linux 7.3.1 comparison on the build host

On November 19, 2026, upstream Linux 7.3.1 compiled for SM-T700 with the previous 6.12.118 ACM probe configuration migrated using `olddefconfig`, NDK r28b, the same RAM init and the same stock S-BOOT TLB patch. The ARM zImage measured 6,348,928 bytes. The disassembled decompressor contained the TLB invalidation immediately after the translation-table update. Both the upstream board DTB (54,157 bytes) and the single-port test DTB (54,181 bytes) matched the corresponding 6.12.118 outputs byte for byte. This build did not use the SM-T280 donor series. [Upstream 7.3.1 source](https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.3.1.tar.xz) · [SM-T700 board definition](https://github.com/gregkh/linux/blob/v7.3.1/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dts)

The separately tested SM-T280 used a Spreadtrum port. Its USB console reported a running 7.3.1 kernel and read-only eMMC. The SM-T700 has an Exynos5420 and uses DWC3. The SM-T700 7.3.1 assessment ended at host compilation. Hardware boot, USB enumeration and Android userspace remain unverified. No Android BOOT container was made or written for this comparison.

This configuration is derived from the upstream Linux kernel. See [COPYING](COPYING) and the referenced kernel license files.
