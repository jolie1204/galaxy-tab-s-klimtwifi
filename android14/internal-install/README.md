# SM-T700 internal storage installation with Linux 6.12.118

This installer is for the SM-T700 (`klimtwifi`) with TWRP 3.7.0_9-0 and the original Samsung partition layout. It uses the Android 14 LineageOS 21 filesystem and native Exynos display configuration verified in [trial 69](../home-trial/README.md#trial-69). The installer writes BOOT and SYSTEM and leaves the existing ext4 USERDATA partition in place.

## Firmware inputs

- Start with the Linux 6.12.118 Android Home kernel and the corresponding Android 14 filesystem. Keep the verified Exynos device tree, launcher, graphics HAL, panel, composer, Android USB HAL and the quarter-scale screen capture path.
- Apply [the first-stage change](native-storage.patch) to the Home first-stage init and use [the internal mount implementation](mount-trial.h). The init code verifies the exact SYSTEM and USERDATA partition names and coordinates, turns on kernel write protection for SYSTEM, and mounts USERDATA as ext4. The candidate expects unencrypted USERDATA.
- Apply [the Android init changes](persistent-init.patch) to the trial 56 framework rc and include [the Home observer](persistent-home.sh) as `/system/bin/klimt-framework-poll.sh`. This starts from the real profile and calls the normal Android services to observe Home. After 900 seconds, the pre-SELinux watchdog reads the standard Android boot-completed property using the installed policy. It can request TWRP if the framework has not completed startup. The launcher and native frame checks are logged independently and require no persistent receipt.
- Use the stock Linux 6.12.118 SELinux filesystem and printk implementations in place of the trial gates and logging hooks. Apply [the MMC change](mmc-write-support.patch) if starting from the diagnostic MMC overlay. The board-specific kernel arguments retain the single-CPU bring-up configuration, use `androidboot.hardware=universal5420`, and assign serial `SMT700-INSTALL` to ACM for passive boot observation.
- The device candidate keeps Google Play and Play services already in its local filesystem. The four optional preloads removed are ExactCalculator, Eleven, Recorder and Etar. Use your own authorized input images and signing materials.

## Create the installation archive

Provide a full 8,388,608-byte Android boot v0 image with the Samsung trailer and a full 2,527,068,160-byte raw ext4 SYSTEM image. The builder checks both inputs, uses SignApk to sign the full ZIP and then verifies the exact packaged image hashes. The signer executable must support the SignApk command-line syntax, including `-w`.

```sh
python3 android14/internal-install/build_package.py \
  --boot work/boot.img --system work/system.img \
  --signapk work/signapk --cert work/cert.x509.pem --key work/key.pk8 \
  --output work/lineage-21-klimtwifi-linux612.zip
```

Before installing, preserve verified offline partitions, especially BOOT, SYSTEM and USERDATA, outside the tablet. Place the ZIP on an external microSD card and confirm its SHA-256. The installer checks the exact recovery and partition layout and both payload hashes before writing. It verifies SYSTEM after writing and completes the BOOT write last. The backups allow manual recovery if power or USB fails during a write. TWRP and the owner data are outside the installer write set.

## Physical installation on 2026-11-21

TWRP 3.7.0_9-0 verified both complete partition writes. The revised BOOT and untouched original RECOVERY were hashed again before boot. Existing USERDATA mounted as writable ext4, while SYSTEM remained read-only. The device reported Android boot completion at kernel uptime 122.727 seconds and later logged the resumed Google setup activity. The owner observed a continuously black internal panel. Treat this build as a diagnostic installation until visible output and input have been verified on the device. The physical device offered only the ACM log transport during this boot. The installed watchdog independently confirmed the boot-completion property after 905.626 seconds of kernel uptime.

The two installed payloads match the local signed r2 archive byte for byte. The ARM bootloader replay passed with valid, invalid and missing ATAG cases. The archive passed detached whole-file CMS verification with the exact AOSP test certificate. The r2 ZIP SHA-256 is `e27831800195875e6245968d27985097cf80f404990d00d5e65e1f50f9fb5edd`. The off-device copies of the original BOOT, SYSTEM and USERDATA were checked against the original device hashes. The backup of USERDATA also passed a read-only ext4 check.

Visible display output, touch, Android ADB, Wi-Fi, Bluetooth and battery reporting remain unverified. Keep the original TWRP and verified backups available.


## Direct display check with the stock-calibrated 105-nit setting

On the same SM-T700, raising the fixed S6E3HA1 calibration from the initial 2-nit row to the device's stock 105-nit row produced visibly readable Linux console output. The Android trial reached boot completion at 124.311 seconds. The owner saw the transition to Android and could identify the battery icon and Back triangle. The main Google Welcome area remained black. This verifies physical panel output and some Android system UI. The Home screen, touch input and the main activity's rendered content remain open.

The trial wrote and checked only the BOOT partition. The factory gamma data remains private. The first 105-nit image retained the regular installation watchdog because its build wrapper delegated the non-root stage to the base script. The corrected build checks the exact embedded Android archive and timer marker after linking, and replays the complete boot image before a device write.

## Synaptics registration and scheduled recovery on the SM-T700

The physical Linux 6.12.118 trial detected the Synaptics S5707 at I2C address 0x20 and registered it as an input device. The DTB uses the existing 3.3 V and 1.8 V supplies, GPX1 pin 6 for the interrupt, and GPG1 pin 1 for the active-low reset line. Touch gesture delivery to Android remains unverified. The [board change](synaptics-klimt.dts.patch) applies to the physical Klimt display-trial DTS. The [RMI I2C change](rmi-i2c-reset.patch) applies to Linux 6.12.118. Enable `CONFIG_RMI4_CORE`, `CONFIG_RMI4_I2C`, `CONFIG_RMI4_F11`, and `CONFIG_RMI4_F12`. The trial does not enable RMI4 firmware updates.

The panel identified itself as `42:11:03`, applied the 105-nit calibration and reported DCS power mode `0x9c`. Android reported boot completion at 124.177 seconds and resumed Google Welcome. At 438.041 seconds the trial requested recovery, and TWRP returned with a new boot ID. Full BOOT and original RECOVERY hashes matched again.

This early boot used an inline capture command whose shell variable was expanded by Android init, and the capture service exited before running the shell. The later trial uses a small init hook and an ordinary shell script.

A second boot with the same BOOT hash was observed using an external camera. Android reported boot completion at 123.623 seconds and Google Welcome resumed at 131.249 seconds. The camera showed the navigation triangle and a black main area while Welcome remained active. The second trial also returned to TWRP by software. The new recovery boot ID, matching partition digests and visible recovery menu were verified. The camera evidence and raw console remain private.


## Paired Android capture, external camera and Synaptics trace

Build the timed diagnostic with [the init hook](panel-capture.rc). It launches [the observer](observe-setup.sh) as the Android shell after boot completion. The trial stages the shell file in `/data/local/tmp/klimt-setup-observe.sh`, owned by UID/GID 2000 with mode 0600. The hook restores the Android label. The observer requires the Synaptics S5707 as the input source, counts position and contact events, and records a native frame while Google Welcome is the resumed activity. The diagnostic continues to request TWRP after 420 seconds.

The packed trial kept the previous kernel code, device tree, timings and boot layout. Its unchanged-size CPIO replacement updated only the Android init file. All three ARM handoff replays passed with the updated archive. Host init verification and the shell syntax check passed, and the fixture verified the capture gate and event counter.

At 169.978 seconds the physical SM-T700 saved a verified 400×640 native capture. The matching camera frame showed the same Back triangle and black main area. The native frame's central rectangle was entirely black. Android reported Google Welcome as the focused app, with `mCurrentFocus=null`. Its scoped window and layer dumps yielded no matching Welcome entry at that point. A fresh TWRP boot and both partition digests were verified after the timed return.

The replay with the same BOOT reproduced the black native center while Google Welcome was the focused app and current focus was null. A later WindowManager sample selected a Bluetooth application error dialog. The observations establish that the black area was present in Android's composed frame. The cause inside the app/window path remains open.

The first input sample returned zero position and tracking events. The owner reported a tap without a verified contact timestamp. The longer replay reached init's 155-second service limit and left its final counter empty. The updated observer keeps the input timeout below that service deadline and writes a checkpoint on the first complete position report. These changes passed the host fixture. Physical touch delivery remains unverified. Both runs returned automatically to fresh TWRP with the expected BOOT and original RECOVERY digests.

## Settings and Trebuchet with the existing internal profile

With the verified 105-nit BOOT, the normal Settings intent returned success and the explicit Trebuchet intent was accepted. ActivityTaskManager later reported the real Trebuchet activity resumed. WindowManager still gave focus to the Bluetooth application error dialog and the default HOME intent resolved to LineageOS Setup Wizard. The camera briefly showed an app splash. The later Android capture and simultaneous camera view both showed a black main area.

A longer BOOT-only comparison extended automatic recovery to 710 seconds and woke the display before capture. It sent Back only while WindowManager reported the exact Bluetooth error dialog. The dialog remained focused when sampled, and the fallback Android activity service returned a broken pipe. The Bluetooth preference was preserved. The camera again reached the Trebuchet splash. Zygote exited with SIGKILL at 424.089 seconds and the late native captures failed. The source of that signal is unconfirmed. The native watchdog requested recovery at 728.214 seconds. A new TWRP boot and the full expected BOOT and original RECOVERY digests verified.
