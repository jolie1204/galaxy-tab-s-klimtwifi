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

The subsequent [early-ADB candidate](early-adb/README.md) preserves the kernel console for recovery readback and starts authenticated ADB from RAM. It has passed ARM emulation and is awaiting SM-T700 testing.

## Linux 7.3.1 comparison on the build host

On November 19, 2026, upstream Linux 7.3.1 compiled for SM-T700 with the previous 6.12.118 ACM probe configuration migrated using `olddefconfig`, NDK r28b, the same RAM init and the same stock S-BOOT TLB patch. The ARM zImage measured 6,348,928 bytes. The disassembled decompressor contained the TLB invalidation immediately after the translation-table update. Both the upstream board DTB (54,157 bytes) and the single-port test DTB (54,181 bytes) matched the corresponding 6.12.118 outputs byte for byte. This build did not use the SM-T280 donor series. [Upstream 7.3.1 source](https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.3.1.tar.xz) · [SM-T700 board definition](https://github.com/gregkh/linux/blob/v7.3.1/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dts)

The separately tested SM-T280 used a Spreadtrum port. Its USB console reported a running 7.3.1 kernel and read-only eMMC. The SM-T700 has an Exynos5420 and uses DWC3. The SM-T700 7.3.1 assessment ended at host compilation. Hardware boot, USB enumeration and Android userspace remain unverified. No Android BOOT container was made or written for this comparison.

This configuration is derived from the upstream Linux kernel. See [COPYING](COPYING) and the referenced kernel license files.
