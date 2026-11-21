# Android 14 Slim for Galaxy Tab S SM-T700 (`klimtwifi`)

**Galaxy Tab S 8.4 Wi-Fi SM-T700 전용입니다.** 실기에서 부팅을 확인한 비공식 LineageOS 21 ROM을 바탕으로 기본 앱 네 개를 뺀 TWRP 설치 ZIP을 만드는 스크립트입니다. 아래 빌드 절차와 실기 검증 기록을 참고하세요.

SM-T700 실기에서 Linux 6.12.118 기반 Android 14의 Trebuchet 홈 화면까지 부팅했습니다. 읽기 전용 microSD와 RAM 데이터로 시험했고, 내장 패널이 켜진 상태에서 400×640 캡처의 실제 RGB 픽셀을 확인했습니다. [69차 실기 기록](android14/home-trial/README.md#trial-69)

The repacker takes one exact retiredtab LineageOS 21 full OTA, modifies its SYSTEM image offline, and rebuilds the TWRP ZIP with the original BOOT payload. The hardware-tested Slim ROM runs Android 14 (API 34) on the base ROM's Linux 3.4.113 kernel. Several patches listed in the maintainer's October build notes were unavailable during the trial, so the repacker uses the published OTA as its input. A separate Linux 6.12.118 RAM diagnostic provided an authenticated USB ADB shell and returned to TWRP by software request. [Hardware trial and board setting](kernel/early-adb/README.md#authenticated-usb-adb-on-linux-612-trial-19)

| Removed from SYSTEM | Retained and checked |
| --- | --- |
| ExactCalculator, Eleven music player, LineageOS Recorder, Etar calendar app | Launcher, Settings, SystemUI, Wi-Fi, browser, WebView, keyboard, calendar and contacts providers, package installer, setup wizard |

The script also removes the base ROM's oversized recovery image and its matching vendor installer. The tested installation used existing TWRP 3.7.0_9-0 for `klimtwifi`.

- [Android 14 source build inputs and hardware-key navigation patch](docs/android14-source.md)
- [Install and recover with TWRP](docs/install.md)
- [Hardware validation, performance and known issues](docs/validation.md)

## Build the installation ZIP

The build was verified on Ubuntu 24.04 x86-64 with Python 3.12, e2fsprogs 1.47, Brotli 1.1, protoc 3.21, Debian/Ubuntu SignApk, and OpenJDK 17. Allow at least 8 GiB of available scratch space. Install the build tools from Ubuntu's package repositories.

```bash
sudo apt update
sudo apt install python3 curl ca-certificates e2fsprogs brotli protobuf-compiler \
  openjdk-17-jre-headless signapk libapksig-java libbcpkix-java libbcprov-java libbcutil-java
```

Create the working directories, then save the exact October 7, 2026 base ZIP in `work/` and keep its filename. [Retiredtab SM-T700 base ROM](https://sourceforge.net/projects/retiredtab/files/SM-T700/21/lineage-21.0-20261007-UNOFFICIAL-klimtwifi.zip/download)

```bash
mkdir -p work/aosp work/scratch
(cd work && sha256sum -c ../checksums/base-rom.sha256)

set -o pipefail
aosp_build='https://android.googlesource.com/platform/build/+/refs/tags/android-14.0.0_r1'
curl -fsSL "$aosp_build/tools/releasetools/ota_metadata.proto?format=TEXT" \
  | base64 -d > work/aosp/ota_metadata.proto
curl -fsSL "$aosp_build/target/product/security/testkey.x509.pem?format=TEXT" \
  | base64 -d > work/aosp/testkey.x509.pem
curl -fsSL "$aosp_build/target/product/security/testkey.pk8?format=TEXT" \
  | base64 -d > work/aosp/testkey.pk8

LC_ALL=C python3 build_slim.py \
  work/lineage-21.0-20261007-UNOFFICIAL-klimtwifi.zip \
  work/lineage-21.0-20261007-SLIM-klimtwifi.zip \
  --proto work/aosp/ota_metadata.proto \
  --signapk-classpath '/usr/share/signapk/signapk.jar:/usr/share/java/*' \
  --cert work/aosp/testkey.x509.pem --key work/aosp/testkey.pk8 \
  --scratch work/scratch
```

The repacker verifies the input checksum and certificate, checks the ext4 filesystem before and after editing, and prints the resulting ZIP checksum. Use that checksum to verify copies of your output. The [hardware-tested ZIP checksum](checksums/hardware-tested-slim.sha256) identifies the original trial artifact. Filesystem edit timestamps and host tools can change the checksum of a new build.

The recovery ZIP uses the same publicly available AOSP test key as the base OTA. The signature can check ZIP integrity. Anyone with the test key can sign a compatible archive. Use the exact source and the verified SHA-256.

## Credits and license

The underlying ROM is the work of retiredtab and the LineageOS, LineageOS-UL, Exynos5420 and Android contributors. This is an unofficial community project. [Upstream Exynos5420 build notes](https://github.com/retiredtab/LineageOS-build-manifests/blob/b74363082f21b2d4a2493e0b36d25d0ae78345e5/21/exynos5420/5420-build-instructions.txt)

The original repacker and project documentation use the [MIT License](LICENSE). The Linux-derived configuration follows the [Linux kernel licensing terms](kernel/COPYING). The ROM, recovery and Google Apps are available from their respective publishers.
