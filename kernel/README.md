# Linux 6.12.118 SM-T700 bring-up experiment

**Cross-compiled. Device boot untested.** The outputs are an ARM `zImage` and the mainline SM-T700 device tree. They still need boot-container, ramdisk and address integration before an Android boot test.

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

This configuration is derived from the upstream Linux kernel. See [COPYING](COPYING) and the referenced kernel license files.
