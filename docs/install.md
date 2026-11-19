# TWRP installation on SM-T700

Use this procedure on the Samsung Galaxy Tab S 8.4 Wi-Fi **SM-T700 (`klimtwifi`)** with a working device-specific recovery. Verify the model before flashing. Prepare a copy of your files and a recovery or rollback path before changing partitions. [Team Win builds for klimtwifi](https://dl.twrp.me/klimtwifi/) · [Device installation and recovery key instructions](https://lineageos.github.io/lineage_wiki/devices/klimtwifi/install/)

The base archive carries a 12,783,632-byte recovery image, which exceeds the tested SM-T700 RECOVERY partition's 10,485,760 bytes. Keep the already working, model-specific TWRP. The Slim ZIP writes SYSTEM and BOOT.

## Flash Android 14 Slim

1. Confirm `ro.product.model=SM-T700` and `ro.product.device=klimtwifi` in the running system, or check the physical model and confirm `klimtwifi` in recovery. With USB ADB already authorized, `adb -s "$DEVICE_SERIAL" reboot recovery` can enter TWRP. Set `DEVICE_SERIAL` to your own device identifier from `adb devices`.
2. Before writing any partitions, back up BOOT, SYSTEM and DATA using your existing recovery, and include EFS if recovery exposes it. Copy the backup and important internal shared files off the tablet and verify that the copies are readable. TWRP's DATA backup can exclude internal shared files. [TWRP data/media documentation](https://twrp.me/faq/datamedia.html)
3. For a migration from Android 10, use TWRP's **Factory Reset** for a clean start. The unwiped migration failed during permission initialization on the test unit. A factory reset erases app state and settings. An in-place update from the identical retiredtab Android 14 base to Slim was tested successfully.
4. Copy the generated Slim ZIP to a location TWRP can read, verify the SHA-256, and choose **Install**. On the tested TWRP, `adb -s "$DEVICE_SERIAL" push "$SLIM_ZIP" /tmp/` and `adb -s "$DEVICE_SERIAL" shell twrp install "/tmp/$(basename "$SLIM_ZIP")"` also worked, with sufficient free space in `/tmp`. Set `SLIM_ZIP` to the generated ZIP path.
5. For optional Google Play, stay in recovery and follow the section below before booting Android. Otherwise reboot to System. The first boot may take several minutes, particularly after a reset. Approve USB debugging again on the tablet if prompted.

If the device still enters TWRP, use a verified backup from that same device for rollback. Raw image restoration was rehearsed on the host. It was not tested on hardware. Use the documented hardware recovery keys or Download Mode when both Android and recovery ADB are unavailable.

## Optional Google Play

Use the unmodified MindTheGapps Android 14 **ARM** package named in [its checksum file](../checksums/mindthegapps-arm.sha256). Install it before first boot. If Android has already booted without Google Apps, the LineageOS guide calls for a factory reset before installing. [LineageOS Google Apps instructions](https://lineageos.github.io/lineage_wiki/gapps/) · [Exact tested MindTheGapps ARM release](https://github.com/MindTheGapps/14.0.0-arm/releases/tag/MindTheGapps-14.0.0-arm-20250203_200051)

On the tested **TWRP 3.7.0_9-0 for klimtwifi**, the add-on installer needed two temporary changes in the recovery environment. It expected `ro.bionic.arch=arm` and a lowercase `system` path. The existing recovery advertised `armeabi-v7a` and exposed `SYSTEM` in uppercase. The following command checks those assumptions before applying either workaround. The compatibility changes live in recovery and disappear after reboot.

```bash
adb -s "$DEVICE_SERIAL" shell '
  set -e
  base=/dev/block/bootdevice/by-name
  test "$(getprop ro.product.device)" = klimtwifi
  test "$(getprop ro.twrp.version)" = 3.7.0_9-0
  test "$(id -u)" = 0
  test "$(getprop ro.product.cpu.abi)" = armeabi-v7a
  test "$(uname -m)" = armv7l
  test -b "$base/SYSTEM"
  test "$(readlink -f "$base/SYSTEM")" = /dev/block/mmcblk0p18
  if [ -z "$(getprop ro.bionic.arch)" ]; then
    setprop ro.bionic.arch arm
  fi
  test "$(getprop ro.bionic.arch)" = arm
  if [ ! -e "$base/system" ] && [ ! -L "$base/system" ]; then
    ln -s "$base/SYSTEM" "$base/system"
  fi
  test "$(readlink -f "$base/system")" = /dev/block/mmcblk0p18
  echo RECOVERY_READY_FOR_GAPPS
'
```

Wait for `RECOVERY_READY_FOR_GAPPS` before installing the checked ZIP. If you staged the Slim ZIP in recovery RAM, reboot directly back to recovery to free that memory, then rerun the compatibility commands. The original Google Apps ZIP and its extracted contents both need space during installation.

After installing the add-on, boot Android, complete the normal setup screens and sign in through Play Store. On the test unit, first startup with Google Apps recorded one system-server memory error and a restart. Setup, sign-in and a Play Store download succeeded afterward. [Observed results](validation.md)
