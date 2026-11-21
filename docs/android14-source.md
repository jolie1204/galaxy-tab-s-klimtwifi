# Android 14 source work for SM-T700

The Android 14 source build targets `lineage_klimtwifi` and Linux 6.12.118. Linux 3.4.113 is the hardware reference from the published October 7 ROM. The existing Slim ZIP repacks that ROM. The November 20 source build produced Android 14 SYSTEM and BOOT images using the 6.12.118 kernel. Both passed host inspection. This image pair has not been installed or booted on the tablet.

A subsequent audit found that the 13:33 Android BOOT omits the board-specific S-BOOT TLB fix. Using the 16 MiB ramdisk offset relative to the observed RAM base also caused the expanded kernel to overwrite the Android ramdisk in a host replay. The physical ramdisk placement remains unmeasured. [Binary comparison and six host replays](../kernel/decompressor/README.md#offline-review-of-the-two-november-20-boot-images)

## Cgroups and the Android property service, trial 22

On November 20, 2026, the installed Android 14 init on the SM-T700 completed its cgroup setup and exercised its protocol 2 property service on Linux 6.12.118. The diagnostic used a minimal early-init action and kept MMC read-only. [Kernel configuration, ramdisk changes and probe](../android14/property-service-trial/README.md)

| Kernel uptime | Physical evidence |
| --- | --- |
| 5.300 s | The installed init entered its second stage. |
| 5.742 s | Init began its built-in cgroup setup. The diagnostic early-init action followed at 5.942 s. |
| 5.955 s | The property socket server identified peer PID 1 and rejected its second write to the probe's write-once property. |
| 5.957 s | Reading the property area returned the original run-specific value, SDK 34 and service protocol 2. |
| 5.959 s | All six controller files resolved on the expected filesystems from the mount namespace of enforcing PID 1, in `u:r:init:s0`. |
| 12.055 s | Linux issued a software restart to recovery. The host confirmed fresh TWRP 3.7.0_9-0 at 21:26:10 PST on the same port. |

Five checked files were on cgroup v1 and covered block I/O throttling, CPU shares, utilization clamps, cpuset and memory limits. A child cgroup's freezer file was on v2. The positive QEMU run produced the same ordered verdicts. A control with the probe script omitted produced no echo or success verdict and requested recovery on its independent timeout.

The real USB ACM capture contained 16,694 bytes. Three reads of TWRP's retained 29,179-byte console agreed. The 7,534,608-byte BOOT file had SHA-256 `7de6deebfb10c6350fbacaa3aa7d86a7d6290b4b054e38c254c7c0a51633e8e0`. The full 8 MiB BOOT partition matched `b456ecb38c7f957f9ca316c345b7b6925af593d7608f625dbee80df4d59b69f7` before the trial reboot and after recovery. The full RECOVERY partition matched its pre-trial digest.

On each of SYSTEM and EFS, the only differences across the trial and TWRP return were six bytes in the primary ext4 superblock. They belonged to mount time, write time, mount count and lifetime KiB written. The remaining bytes of both complete partitions matched their pre-trial SHA-256 comparisons. Both filesystems reported clean in TWRP.

The next hardware checkpoint is starting Android's `ueventd`, observing its coldboot device scan and verifying `ro.cold_boot_done=true` through the property service. Zygote, the framework, the launcher and workload performance remain untested on this kernel.

## SELinux and second-stage init on Linux 6.12, trial 21

On November 20, 2026, the SM-T700 ran its installed Android 14 `/system/bin/init` through `selinux_setup` into `SecondStageMain` on Linux 6.12.118. The PC captured the following sequence over USB CDC ACM. TWRP's retained kernel console independently contained the same trial ID and sequence. [Diagnostic source and controls](../android14/policy-entry-trial/README.md)

| Kernel uptime | Physical evidence |
| --- | --- |
| 4.274 s | First-stage PID 1 verified the read-only internal SYSTEM root before handing off to the installed init. |
| 5.064 s | The kernel accepted the 785,103-byte policy. SHA-256 of the bytes passed to SELinux matched the precompiled policy on the tablet. |
| 5.067 s | The kernel audit recorded Android switching SELinux enforcement on. |
| 5.208 s | The original init logged `init second stage started!`. The kernel measured PID 1's context as `u:r:init:s0` with enforcement enabled. Offline inspection also confirmed that the policy's init domain is enforcing. |
| 11.416 s | Linux restarted with the command `recovery`. TWRP 3.7.0_9-0 was confirmed on the same USB port at 21:11:12 PST with a fresh boot ID. |

The PC captured 12,502 bytes over ACM. Three reads of the 25,522-byte retained kernel console agreed. The diagnostic BOOT image was 7,426,064 bytes, SHA-256 `74b4b2237eb80120321d8675c0141e59a2c748ebc16e77ccaaecc76852d44948`. Full BOOT readback including the retained partition tail matched `6ccdbf5eff0a2adc29ba1568dbd83b61c09e59038011341ab3ce3e0f455569e2`. RECOVERY matched its pre-trial hash.

Across the complete trial and TWRP return, 616,959 of 616,960 SYSTEM blocks and 5,119 of 5,120 EFS blocks matched their pre-trial SHA-256 at 4 KiB granularity. On each partition, only block zero differed. For SYSTEM, a byte comparison found six changed bytes in the primary ext4 superblock fields for mount time, write time, mount count and lifetime KiB written. For EFS the comparison identifies the first block, which contains the primary superblock. TWRP's kernel recorded mounts of both filesystems, and each superblock reported a clean filesystem and a last-write time during recovery startup.

The installed policy (version 30) leaves two Linux 6.12 classes undefined, `mctp_socket` and `user_namespace`, along with the `bpf` and `checkpoint_restore` permissions in two capability classes. The kernel reported that access to these would be denied. A QEMU run allowed second-stage initialization to continue with an empty boot-script selection. Android attempted to mount CPU and memory cgroups that the virtual kernel did not provide. The physical trial kernel configuration also has CPU scheduling groups, cpusets, CPU accounting and the memory controller disabled. Trial 22 subsequently verified the cgroup mounts and the Android property service on hardware. Zygote, the framework and the launcher are outstanding.

## Android 14 first stage on Linux 6.12, trial 20

On November 20, 2026, the physical SM-T700 ran Android 14 first-stage `init` as PID 1 on Linux `6.12.118-klimt-adbreturn1`. It discovered the installed SYSTEM by partition name, set the partition read-only at the block layer and used Android's first-stage mount to switch root. After the switch, PID 1 verified that `/` was read-only ext4 on the internal `mmcblk0p18` and that the same filesystem held an executable ARM32 `/system/bin/init`. This trial paired a source-built first-stage ramdisk with the Android 14 SYSTEM already installed on the tablet. [Diagnostic patch, fstab and build settings](../android14/first-stage-trial/README.md)

| Physical observation | Result |
| --- | --- |
| Host USB capture | CDC ACM enumerated at 20:54:13 PST. The PC received 10,948 bytes through its `/dev/ttyACM1`, including the tablet's native PID 1 marker, mount verdict and recovery request. |
| Kernel uptime | PID 1 marker at 4.091 s, read-only block guard at 4.329 s, ext4 mount without journal replay at 4.343 s. |
| Software recovery | Linux logged the `recovery` restart at 16.420 s. Fresh TWRP 3.7.0_9-0 was confirmed on the same port at 20:54:48 PST. Three reads of its 23,047-byte previous console agreed and contained the same run ID and verdict. |
| Image | The 7,424,016-byte BOOT image had SHA-256 `1655474f4fd54c0d6915676fa9175674a40738cf4a1f253ffac6ad7dbfa34005`. The entire 8 MiB BOOT partition, including its retained tail, matched the projected digest after recovery. |

The RECOVERY partition matched its pre-trial SHA-256. The full SYSTEM and EFS hashes each differed across the reboot, then remained stable across two complete reads in TWRP. TWRP's own kernel logged recovery mounting SYSTEM repeatedly and EFS once. Both ext4 superblocks were clean and recorded writes during TWRP startup. The trial kernel logged its read-only SYSTEM mount, and its fstab contained no EFS entry. The pre-trial hashes alone cannot establish which bytes changed across the entire boot and recovery cycle.

The diagnostic requested recovery immediately after checking the new root. Trial 21 subsequently reached SELinux policy load and second-stage entry. The authenticated ADB shell ran in the preceding trial. [Trial 19 USB ADB results](../kernel/early-adb/README.md#authenticated-usb-adb-on-linux-612-trial-19)

## SM-T700 hardware ID for Android first-stage mount

The current standard and embedded Android BOOTs supply `universal5420` in the board's `/firmware/android/hardware` property. Android 14's libfstab reads this property when the parent is compatible with `android,firmware`. This selects the existing `fstab.universal5420` even when Samsung supplies other command-line arguments and omits the hardware ID. Android's second-stage init also imports properties from this node. [Current BOOT and paired SYSTEM checksums](../checksums/android14-firmware-dt-20261120.sha256)

1. Apply the [Android firmware DTS patch](../android14/klimtwifi-android-firmware.patch) and the [S-BOOT TLB fix](../kernel/armv7-sboot-tlb.patch) in the Linux 6.12.118 source. Build the ARM zImage and `samsung/exynos5420-klimt-wifi.dtb` with the Android configuration below. Concatenate zImage followed by this DTB into `device/samsung/klimtwifi-kernel/zImage-dtb` in the Android checkout.
2. Apply the [build-time firmware check](../android14/klimtwifi-boot-firmware-check.patch) in the Android checkout after the prebuilt and BOOT layout patches below. Copy the [validator](../kernel/android_firmware.py) to `device/samsung/klimtwifi-kernel/android_firmware.py` alongside the prebuilt. Run the product's `m bootimage`. The build uses its own host `fdtget` to check the board and Android properties on the kernel it will package.
3. For independent verification add the Android checkout's `out/host/linux-x86/bin` to `PATH` and run `python3 check-linux612-boot.py --android BOOT ZIMAGE DTB IMAGE` from this repository's `kernel/` directory. The embedded assembler below also runs this check on its final image.

As a regression test, the normal `bootimage` build received the previous prebuilt. It stopped at the missing firmware node before packaging. Restoring the corrected prebuilt produced the archived BOOT digest again. The verifier also rejected an incorrect hardware value and the sibling board.

In Cortex-A15 emulation, an Android first-stage boot with a nonempty command line missing the hardware ID reproduced the fstab failure with the former tree. Four runs with the corrected board properties reached the EFS and SYSTEM partition wait. Those runs covered an omitted ID, the correct ID, two copies of the correct ID, and a conflicting ID. The guest used the Android ramdisk and a virtual board without the tablet's storage. Three separate replays of the packaged tablet images verified the complete expanded kernel and both firmware nodes at handoff, including the embedded build with the external initrd tag omitted. The device-tree change preserved the existing Samsung secure firmware node. The sibling Chagall Wi-Fi board compiled to its original DTB byte for byte.

Trial 19 subsequently captured the tablet's partition uevents, including EFS on p3 and SYSTEM on p18. Trial 20 verified the by-name SYSTEM link and the first-stage root switch. The standalone early-ADB probe uses a separate RAM init and command line. [First-stage hardware result](#android-14-first-stage-on-linux-612-trial-20)

## Corrected Linux 6.12 BOOT images, November 20

The first layout-corrected `bootimage` packaged the S-BOOT TLB fix and requested a ramdisk 128 MiB above the header base. An Android build with a built-in ramdisk also carried the complete first-stage userspace when the loader omitted the initrd tag. These earlier Android images used the empty-command-line hardware fallback. The independent diagnostic from this build remains the archived early-ADB candidate. [November 20 layout and diagnostic image checksums](../checksums/linux612-hostfix-20261120.sha256)

| Local build output | Size | Userspace at kernel handoff |
| --- | ---: | --- |
| android-boot.img | 7,766,032 bytes | Original 1,382,400-byte Android XZ ramdisk at the modeled +128 MiB placement |
| android-embedded-boot.img | 7,768,080 bytes | Complete 2,972,416-byte Android newc archive built into the kernel, plus a valid 92-byte empty external XZ archive |
| diagnostic-boot.img | 6,885,392 bytes | Run-specific decompressor probes, built-in authenticated ADB and recovery timer |

For Android, apply the [board's TLB patch](../kernel/armv7-sboot-tlb.patch) to the Linux 6.12.118 source before building the prebuilt zImage. Merge [the Android config fragment](../android14/linux-6.12-android-ipc-security.config) over the Exynos configuration. When the bootloader supplies no command line, the fallback now passes `androidboot.hardware=universal5420`. This removes the reference board's `mem=256M`, fixed initrd address and `init=/linuxrc` defaults. The normal bootloader command line still takes precedence.

Apply [the BOOT layout patch](../android14/klimtwifi-boot-layout.patch) to the Android source root using GNU patch after the prebuilt patch. It also uses the 10 MiB RECOVERY partition size previously measured by TWRP. The generated build graph used +128 MiB for BOOT and +16 MiB for RECOVERY. Only BOOT was rebuilt. For an ordinary build run `m bootimage` after selecting the product with lunch. Use the [BOOT checker](../kernel/check-linux612-boot.py) with the platform BOOT, ARM zImage, tablet DTB and expanded Image, in that order. It checks the packed TLB sequence, payloads, v0 ID, partition capacity, trailer and modeled address intervals.

To reproduce the embedded Android variant, first unpack the corrected platform BOOT with the checkout's `system/tools/mkbootimg/unpack_bootimg.py`. Decompress its ramdisk using `xz -dc ramdisk > android.cpio`. In a separate kernel output directory use the same corrected kernel config with `CONFIG_INITRAMFS_SOURCE` set to the absolute path of that cpio and `CONFIG_INITRAMFS_COMPRESSION_NONE=y`. Keep XZ compression of the outer kernel. Build zImage and run the [assembler](../android14/assemble-embedded-boot.py) as `python3 assemble-embedded-boot.py PLATFORM_BOOT KERNEL_OUT MKBOOTIMG_DIR OUTPUT`, passing the checkout's `system/tools/mkbootimg` directory and a distinct output filename. The assembler verifies the exact Android archive inside Image, round-trips the original BOOT, retains its DTB and Android security patch level, and runs the BOOT checker on the result.

Cortex-A15 replays reached the expanded Android image in three cases covering the platform BOOT with an initrd tag, and the embedded BOOT with the tag present and absent. The RAM contents matched their complete build reference images. The tagged paths reported the exact modeled bounds in /chosen. With the tag absent the same Android first-stage archive was present in the embedded Image. A separate virt guest used the Android config with virtual-board and serial-console support enabled. With SELinux active and no external initrd it ran the Android ramdisk's first-stage /init, copied its ramdisk properties, and waited for EFS and SYSTEM. No tablet partitions were attached to the guest and first-stage mount stopped there. The tablet fstab uses the legacy dw_mmc.0 path. The mainline DT describes the eMMC controller at 0x12200000. Compare the tablet's actual partition uevents and links in early ADB before changing mount paths. Physical addresses used by S-BOOT and Android startup on the tablet remain unverified. [Diagnostic tests and QEMU limits](../kernel/decompressor/README.md#host-prepared-diagnostic-with-built-in-adb)

## Verified image pair, November 20, 2026

The `systemimage bootimage` build completed at 13:33 PST for `lineage_klimtwifi-ap2a-userdebug`, Android 14 / API 34, build ID `AP2A.240905.003`. No device write was performed. The separately instrumented 6.12 kernel reached its decompressor entry in an earlier hardware test. Boot of this Android image, ADB, physical navigation, graphics, Bluetooth operation and memory behavior still require hardware testing. [Image SHA-256 checksums](../checksums/android14-linux612-20261120.sha256) · [Hardware checkpoint](../kernel/decompressor/README.md#thirteenth-trial-with-both-linux-612-decompressor-checkpoints)

| Artifact | Host verification |
| --- | --- |
| `boot.img`, 7,763,984 bytes | Android boot v0 header. Kernel, ramdisk and tags addresses match the October reference. The kernel payload exactly matches the 6.12.118 zImage plus tablet DTB. The XZ ramdisk contains a 32-bit ARM init and the device fstab. The Samsung trailer is at the aligned payload boundary. 624,624 bytes remain in the 8 MiB partition. |
| `system.img`, 1,541,325,308 bytes | Android sparse v1 expands to 611,840 ext4 blocks of 4,096 bytes (2,506,096,640 bytes), exactly the declared SYSTEM partition size. A read-only five-pass `e2fsck` completed successfully. The image's properties report Android 14 and SDK 34. |

All 167 standalone APKs read from the ext4 filesystem matched their staged SHA-256 values. The final image retains Jelly, WebView, the calendar provider, LatinIME, Trebuchet, Settings, SystemUI and the Bluetooth APEX. The four optional apps and the Google Play Store/Play services packages are absent. The extracted device overlay sets the initial taskbar opt-out flag to true, with HOME (172), BACK (158) and APP_SWITCH (254) in the installed key layouts. The October ROM's recovery installer and embedded recovery image paths are absent from SYSTEM.

### Source selection and local patches

The [locked manifest](../android14/lineage21-klimtwifi-revisions.xml) records all 1,471 project revisions used for the build, before the local patches. It uses an absolute public GitHub fetch URL. Initialize and sync a clean checkout with the project manifest.

```bash
repo init -u https://github.com/jolie1204/galaxy-tab-s-klimtwifi.git -b main \
  -m android14/lineage21-klimtwifi-revisions.xml --depth=1 --git-lfs --no-use-superproject
repo sync -c
```

Apply the local [Slim package](../android14/klimtwifi-slim-apps.patch), [device taskbar](../android14/klimtwifi-taskbar-default.patch), [6.12 prebuilt and Samsung header](../android14/klimtwifi-prebuilt-linux612.patch), [Lineage observer](../android14/lineage-user-switch-observer.patch), [RIL header](../android14/klimtwifi-ril-headers.patch) and [FIMG](../android14/exynos-fimg-instance.patch) patches from the source root. Apply the pinned framework update and place the kernel payload as described below.

The following maintainer patches applied cleanly inside the corresponding projects. The RIL include patch above supplies the `hardware/ril/include` path to Samsung audio and both client libraries while retaining their existing install partitions. All three produced ARM shared objects in the build. [Maintainer patch repository at the exact revision](https://github.com/retiredtab/LineageOS-build-manifests/tree/b74363082f21b2d4a2493e0b36d25d0ae78345e5/21)

| Android project directory | Patch under the maintainer's `21/` directory |
| --- | --- |
| `frameworks/av` | `exynos5420/5420-frameworks-av-omx-codecs.diff` |
| `hardware/lineage/interfaces` | `UL-patches-2024/hardware_lineage_interfaces.diff` |
| `hardware/samsung_slsi/exynos` | `exynos5420/hardware_samsung_slsi_exynos.diff` |
| `hardware/samsung_slsi/exynos5420` | `exynos5420/hardware_samsung_slsi_exynos5420.diff` |

## Build inputs

The maintainer uses LineageOS-UL `lineage-21.0` and the Exynos5420 local manifest. The manifest selects the klimtwifi, klimt-common and universal5420-common device trees, older Exynos5420 vendor branches and a downstream Samsung kernel. On November 19 a filename comparison against the complete public manifest repository found 10 of the 24 patch filenames referenced by the build instructions absent at both the October revision and the head checked. These included the October platform manifest patch, July framework patch and Supercell shim patch. The comparison does not detect equivalent changes already merged or distributed under other names. Reconstruct or account for those changes and pin the resulting revisions before describing a build as reproducing the October release. [Maintainer instructions](https://github.com/retiredtab/LineageOS-build-manifests/blob/b74363082f21b2d4a2493e0b36d25d0ae78345e5/21/exynos5420/5420-build-instructions.txt) · [October device manifest](https://github.com/retiredtab/LineageOS-build-manifests/blob/b74363082f21b2d4a2493e0b36d25d0ae78345e5/21/exynos5420/5420-21.xml) · [Repository head checked](https://github.com/retiredtab/LineageOS-build-manifests/tree/ce6ca6f54c51d08be70243c7069a769aa474ef02)

## Linux 6.12 and Android 14 host build, November 20

A separate checkout uses the [LineageOS-UL platform manifest](https://github.com/LineageOS-UL/android/tree/a156f53737d9a413581b125ad81a0d9d62f50036) and the October device manifest above. The initial 20-project source trial included the SM-T700 device, shared Exynos hardware, vendor trees, Bionic and Android init. Those initial component tests ran without checking out the downstream kernel. The later full-platform build checked it out for header generation.

**Kernel selection.** The inherited board file still sets `TARGET_KERNEL_SOURCE := kernel/samsung/exynos5420` and `TARGET_LINUX_KERNEL_VERSION := 3.4`. Lineage's build rules prefer that source when present. Several Exynos HAL makefiles consume headers under `KERNEL_OBJ/usr`, and the device rules name `/dev/ion` and `/dev/mali0`. Selecting a 6.12 kernel binary alone does not provide those interfaces. The device BOOT rules now package the Android ramdisk and verified 6.12 kernel. Runtime validation of the selected graphics stack and its ioctls on those drivers remains pending. [Board configuration](https://github.com/retiredtab/android_device_samsung_universal5420-common/blob/b8f7784abffb34bc1c9639ade1603344d37f3bb3/BoardConfigCommon.mk) · [Kernel selection rules](https://github.com/LineageOS-UL/android_vendor_lineage/blob/2fed97c50c004e78c1691bdf3620ac2dcb21a711/build/tasks/kernel.mk) · [Exynos hardware composer](https://github.com/LineageOS/android_hardware_samsung_slsi_exynos/blob/fb9857c5aa8923cc043b8f8a91bd24339c4bf528/libhwc/Android.mk)

**Binder and SELinux.** Merge the [Android IPC and security fragment](../android14/linux-6.12-android-ipc-security.config) over the [archived ARM config](../kernel/exynos_xz.config) with `scripts/kconfig/merge_config.sh -m` in a separate output directory, then run `olddefconfig` as in the [kernel build instructions](../kernel/README.md). Every fragment setting survived resolution, and the default security module list included SELinux. Binder, binderfs, SELinux, ext4 security labels and the BPF syscall compiled as ARM EABI5 objects. The checked Android init mounts binderfs and uses the binder, hwbinder and vndbinder nodes. [Android init](https://github.com/LineageOS-UL/android_system_core/blob/cf103cae1bd46f6be19d1e407a6ea55baa38cb83/rootdir/init.rc#L258)

An ARMv7/API 34 compile compared Android's Bionic header with the exported Linux 6.12.118 ARM header. Both produced Binder protocol 8, an 8-byte Binder pointer field, a 48-byte `binder_write_read`, a 64-byte `binder_transaction_data` and identical values for `BINDER_VERSION`, `BINDER_WRITE_READ`, `BC_TRANSACTION` and `BR_TRANSACTION`. On-device Binder transactions, SELinux policy loading, graphics and proprietary blob compatibility still require a running kernel and Android init. [Android Binder header](https://github.com/LineageOS-UL/android_bionic/blob/eae9b76f8bb4752cdd4faada596aa5b06237051d/libc/kernel/uapi/linux/android/binder.h)

**BOOT space.** A separate build from upstream Linux 6.12.118 produced a 6,324,984-byte ARM `zImage` (SHA-256 `86a258e3d311569c70caee82be4108b600b461629f93154c55dbe46a5717e275`) and a 54,157-byte SM-T700 DTB (SHA-256 `f055db32fa52a210614e176de0b035ad4eb9dc85b7c48bdfe96fb58e597b1b71`). The October ROM has a 1,381,840-byte compressed ramdisk, a 2,048-byte header page and a 16-byte Samsung trailer. Substituting the new kernel and appended DTB in that layout estimates 7,763,984 bytes, leaving 624,624 bytes in BOOT. This estimate uses the old ramdisk size. The separate zImage was initially left unpackaged. The final BOOT measurement is recorded above. S-BOOT handoff of the Android image remains untested. Hardware entry and USB ADB are being tested with the [separate instrumented diagnostic](../kernel/decompressor/README.md).

**Slim product.** Apply the [package selection patch](../android14/klimtwifi-slim-apps.patch) from the Android source root with `patch -p1`. GNU Make evaluation of the two changed Lineage product files removed exactly ExactCalculator, Eleven, Etar and Recorder for `lineage_klimtwifi`. The equivalent comparison for `lineage_chagallwifi` produced an identical package set before and after the patch. CalendarProvider is separately requested in the Android handheld system configuration. The completed filesystem inventory confirms the four omissions. Google Play is a separate installation. The device-specific taskbar patch below was also applied in this checkout. AAPT2 34.0.0 compiled and linked the provider resources with the taskbar opt-out flag false for the base and true with the device overlay.

### Selecting the 6.12 payload in the Android build

From the Android checkout root, apply [the BOOT and header selection patch](../android14/klimtwifi-prebuilt-linux612.patch) with `patch -p1`. Put the previously built ARM zImage followed by the SM-T700 DTB in the device prebuilt directory.

```bash
mkdir -p device/samsung/klimtwifi-kernel
cat "$kernel_out/arch/arm/boot/zImage" \
    "$kernel_out/arch/arm/boot/dts/samsung/exynos5420-klimt-wifi.dtb" \
    > device/samsung/klimtwifi-kernel/zImage-dtb
sha256sum device/samsung/klimtwifi-kernel/zImage-dtb
```

The original 13:33 build payload was 6,379,141 bytes, SHA-256 `20992d894f626e8849ca4d462a6a7cbe4ff74038f1754cd072f6cf69a51526e5`. With the downstream Samsung source both present and absent, a Make check of the patched board and Lineage kernel rules selected this file, set `FULL_KERNEL_BUILD=false` and copied identical bytes. The device opts into Lineage's generated header substitution for legacy Make HALs. A comparison checked that both the existing inline-build path and the new device option resolve those dependencies to `generated_kernel_headers`. A standalone ARM `headers_install` exported 668 headers from Samsung revision `ece5708530bb80a85b26edeee568733b14740239`. The subsequent platform build targets `lineage_klimtwifi`, release `ap2a`, variant `userdebug` and the `systemimage bootimage` goals. Inspect the kernel payload in the completed BOOT before any hardware test.

### Platform compilation

The complete checkout synced 1,471 projects. The manifest pinned each project to a commit before compilation. The build uses `source build/envsetup.sh`, `lunch lineage_klimtwifi-ap2a-userdebug` and `m systemimage bootimage`. The Lineage lunch environment sets `LINEAGE_BUILD` and loads the platform's Soong namespaces. Invoking Soong with only AOSP target variables omitted the FM JNI namespace and failed during graph generation.

The full graph selected the prebuilt payload for BOOT and produced Samsung kernel headers for the HAL build. During Java compilation, the Lineage SDK implemented a two-argument pre-switch callback while the platform expected one argument. The [user-switch observer patch](../android14/lineage-user-switch-observer.patch) uses the framework's `UserSwitchObserver` adapter and keeps the settings refresh in `onUserSwitchComplete`. The framework handles the switch acknowledgement. The previously failing `org.lineageos.platform` Java target then produced a 362,129-byte JAR and the image build resumed with the existing intermediate files.

The checked-out Trebuchet and IntentResolver also require framework APIs added after the April 2025 UL framework revision. These include the launcher's image-decoding limits, the Home key call to SystemUI and intent resolution with an explicit MIME type. To keep the current clients and the Exynos compatibility changes, the build incorporates the official LineageOS 21 framework update onto the UL tree. Comparing the UL revision with its Lineage merge parent identified 29 files changed for UL. Applying that delta to the newer Lineage tree independently produced the same tree hash as applying the Lineage update to UL. [UL merge](https://github.com/LineageOS-UL/android_frameworks_base/commit/aca8bb27401d94ac4521043e13f414e19f51bd3c) · [Pinned upstream framework](https://github.com/LineageOS/android_frameworks_base/commit/631771503aae14fe05f40bc0e4b850548bc39aa5)

Run this from a freshly synced `frameworks/base` for the pinned UL revision. The last check verifies all tracked framework content, including the UL changes.

```bash
set -eo pipefail
test "$(git rev-parse HEAD)" = aca8bb27401d94ac4521043e13f414e19f51bd3c
parent=cf4daf150e0066abdaeffa6dd5bba6f7ebce2b42
upstream=631771503aae14fe05f40bc0e4b850548bc39aa5
git fetch --no-tags --depth=1 https://github.com/LineageOS/android_frameworks_base.git "$parent" "$upstream"
git diff --binary "$parent" "$upstream" | git apply --3way --whitespace=nowarn
test "$(git write-tree)" = 664dd37bc5a4a01affb0bd7811be981bdcc7caae
```

With this framework, the previously failing `IntentResolver-core` and `Launcher3QuickStepLib` Java targets produced JARs of 5,367,370 and 15,300,266 bytes respectively. Hardware Home, Recents and share-sheet behavior still require device testing.

**Exynos graphics HAL.** Android's Clang stopped on an unreachable loop increment in the selected `libfimg4x` implementation. Every path through the first iteration already exited the function. The [Exynos FIMG patch](../android14/exynos-fimg-instance.patch) accesses the current slot directly and advances only after a successful create or reuse. The SM-T700 configuration uses one slot. A host check using the original and patched function bodies with a simulated device verified out-of-range handling, failed creation and retry, instance count, reuse, and wraparound with one and three slots. The Android build then compiled the object for 32-bit ARM. Display behavior still requires hardware validation.

## Hardware keys and tablet taskbar

The installed key layouts map code 172 to HOME, 158 to BACK and 254 to APP_SWITCH. The kernel advertises the home key through gpio-keys and the last two through sec_touchkey. The resolved framework resource config_showNavigationBar was false and Lineage force_show_navbar was 0. Window Manager identified a separate bottom Taskbar owned by com.android.launcher3. There was no stored Lineage enable_taskbar preference. The inspected Trebuchet code defaults this preference to enabled on tablets. The awake welcome-screen accessibility hierarchy showed only SetupWizard. Testing the exact visible buttons and physical key actions in the launcher and apps is still pending. [Device key layout](https://github.com/retiredtab/android_device_samsung_klimt-common/blob/bf398b984517260fd5d95ff203d209e34bb8887f/configs/keylayout/sec_touchkey.kl) · [Trebuchet tablet default](https://github.com/LineageOS/android_packages_apps_Trebuchet/blob/08002b5483685be3457908456afb42277eed4e08/src/com/android/launcher3/DeviceProfile.java#L353)

The [source patch](../android14/klimtwifi-taskbar-default.patch) sets the initial Lineage enable_taskbar preference to 0 when creating a settings database for klimtwifi. Users can subsequently change their preference. It uses the device's existing product overlay directory and LineageSettingsProvider's INSERT OR IGNORE initialization. Existing installations retain their settings. Apply the patch from the Android source root with `patch -p1 < /path/to/klimtwifi-taskbar-default.patch`. The patch applied to the checked source inputs. AAPT2 34.0.0 compiled both resource sets and an isolated link resolved the opt-in flag to false for the base and true for klimtwifi. The full platform build subsequently linked the device's vendor resource overlay. On-device behavior remains unverified. After setup, check portrait and landscape in the launcher and Settings, then press each physical key and verify the resulting action. [Device overlay declaration](https://github.com/retiredtab/android_device_samsung_klimtwifi/blob/8dbcf8aba9efe773ed2230819dddc843aa19c71c/device.mk) · [Settings provider source](https://github.com/LineageOS/android_lineage-sdk/blob/e281b169a0091e6647a4250fa0001f3fb350d872/packages/LineageSettingsProvider/src/org/lineageos/lineagesettings/LineageDatabaseHelper.java)

## Memory failures on the existing Android build

On November 19, system_server raised OutOfMemoryError in native Parcel string writes during an account-related operation and, on a later boot, during package and Wi-Fi results. The captured exceptions did not report the failing allocation size. The running build uses zygote32. The 192 MiB heap growth limit and 512 MiB maximum match the klimt-common source properties. Free memory at clean setup was measured at a different time from the crashes. The clean post-reset checks found no OOM during their observation windows. These observations leave the cause open. Compare boot and the same package-query/Wi-Fi workload with identical setup and Play components on the existing and source-built ROMs. Collect timestamped system_server memory, system memory and crash logs together before changing heap limits. [Device heap settings](https://github.com/retiredtab/android_device_samsung_klimt-common/blob/bf398b984517260fd5d95ff203d209e34bb8887f/system_prop.mk) · [Observed clean setup](validation.md#clean-setup-after-the-kernel-trials)
