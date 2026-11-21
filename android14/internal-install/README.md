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

TWRP 3.7.0_9-0 verified both complete partition writes. The revised BOOT and untouched original RECOVERY were hashed again before boot. Existing USERDATA mounted as writable ext4, while SYSTEM remained read-only. The device reported Android boot completion at kernel uptime 122.727 seconds and later logged the resumed Google setup activity. The owner observed a continuously black internal panel. Treat this build as a diagnostic installation until visible output and input have been verified on the device. The physical device offered only the ACM log transport during this boot.

The two installed payloads match the local signed r2 archive byte for byte. The ARM bootloader replay passed with valid, invalid and missing ATAG cases. The archive passed detached whole-file CMS verification with the exact AOSP test certificate. The r2 ZIP SHA-256 is `e27831800195875e6245968d27985097cf80f404990d00d5e65e1f50f9fb5edd`. The off-device copies of the original BOOT, SYSTEM and USERDATA were checked against the original device hashes. The backup of USERDATA also passed a read-only ext4 check.

Visible display output, touch, Android ADB, Wi-Fi, Bluetooth and battery reporting remain unverified. Keep the original TWRP and verified backups available.
