# Android 14 source work for SM-T700

The Android 14 source build targets `lineage_klimtwifi` and Linux 6.12.118. Linux 3.4.113 is the hardware reference from the published October 7 ROM. The existing Slim ZIP repacks that ROM. The November 20 source build produced Android 14 SYSTEM and BOOT images using the 6.12.118 kernel. Both passed host inspection. This image pair has not been installed or booted on the tablet.

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

The November 20 payload is 6,379,141 bytes, SHA-256 `20992d894f626e8849ca4d462a6a7cbe4ff74038f1754cd072f6cf69a51526e5`. With the downstream Samsung source both present and absent, a Make check of the patched board and Lineage kernel rules selected this file, set `FULL_KERNEL_BUILD=false` and copied identical bytes. The device opts into Lineage's generated header substitution for legacy Make HALs. A comparison checked that both the existing inline-build path and the new device option resolve those dependencies to `generated_kernel_headers`. A standalone ARM `headers_install` exported 668 headers from Samsung revision `ece5708530bb80a85b26edeee568733b14740239`. The subsequent platform build targets `lineage_klimtwifi`, release `ap2a`, variant `userdebug` and the `systemimage bootimage` goals. Inspect the kernel payload in the completed BOOT before any hardware test.

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
