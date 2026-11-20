# Validation on one Galaxy Tab S SM-T700

## Android 14 Slim, November 18–19, 2026

The inputs and [hardware-tested ZIP checksum](../checksums/hardware-tested-slim.sha256) identify the exact trial. The published retiredtab base build is dated October 7, 2026. Its SYSTEM reported Android 14/API 34, and BOOT used the 3.4.113 Deathly kernel.

| Check | Observed result |
| --- | --- |
| Offline rebuild | Clean `e2fsck -fn` and valid whole-file AOSP test-key signature. The signed ZIP's SYSTEM reconstructed to the verified block image. |
| Footprint | 658,469,922-byte Slim ZIP versus 694,436,055-byte base ZIP. Free ext4 space in SYSTEM increased by 46.5 MiB. |
| TWRP installation | Existing TWRP wrote the expected 394,737 system blocks and reported success. BOOT matched the pre-installation whole-partition hash because the trial started from the identical retiredtab BOOT payload. RECOVERY also retained its pre-installation hash. |
| Same-build upgrade | USERDATA UUID, secure setup settings file and all 45 existing shared file digests matched before first Android boot. |
| Normal boot | Boot completed. The launcher opened, existing setup was retained, and the saved Wi-Fi reconnected with verified internet access. |
| App removal | Android Package Manager confirmed the four target packages were absent. WebView, network stack, keyboard, launcher, Settings, setup and the retained providers were present. |
| Optional Google Play | Google Apps installed after a factory reset. Play Store sign-in completed and the storefront opened. The Play Store installed meonmesh Wemo Controller 4.4.0 (45). The app opened cold in 1,998 ms in a single test. |

The initial unwiped migration from Android 10 did not complete. The existing user-0 data path triggered an SELinux denial during permission migration. A clean Android 14 boot succeeded after factory reset. First setup with Google Apps recorded one `system_server` `OutOfMemoryError` while parceling Package Manager results and a restart. The underlying cause remains unverified. Setup and Play Store download succeeded after the restart.

Wemo LAN discovery and control of a physical load were outside the installation check. Long-run stability, unplugged battery life, other tablet variants and a full-device restore were untested. The [subsequent BOOT-only restore](../kernel/probe.md#android-14-restoration) was verified on this tablet.

On November 19, a fresh host build from the published script passed ext4, ZIP CRC and whole-file signature checks. The BOOT payload, transfer list and updater matched the hardware-tested ZIP. The fresh output SHA-256 was `4f4898ea8334b3d51621aba5661834249a800b8b0c583872a62fff2416544c6c`. The hardware tests above used the earlier archived ZIP.

## Clean setup after the kernel trials

The November 19 TWRP factory reset retained the internal shared files (all regular-file hashes matched) and the unmounted microSD. BOOT and RECOVERY matched their original hashes. The original Android 14 Slim reached the Google welcome screen on Linux 3.4.113. Both setup flags were zero. Play Store and Play services remained installed, with no third-party user apps or accounts. Bluetooth reported ON. The same system_server process was present for over two minutes after boot completion, with no system-process fatal exception or out-of-memory entry in the crash buffer at the final check. Longer-term stability and the earlier restart cause remain unverified.

## Short performance samples before Google Apps

USB connected, screen on and four online cores. Launch times are medians of five runs. Scrolling used 18 repeated swipes on the 60 Hz device.

| Measurement | Base Android 14 | Android 14 Slim |
| --- | ---: | ---: |
| Home CPU busy, 30 seconds after settling | 3.40% | 3.33% |
| Home available memory, median | 1.41 GiB | 1.59 GiB |
| Settings network screen, process-cold | 2.305 s | 2.260 s |
| Settings network screen, process still resident | 0.226 s | 0.287 s |
| Settings frames that missed their deadline | 77/736 (10.5%) | 69/741 (9.3%) |

The idle tests began roughly 24 minutes and 2.5 minutes after boot, respectively. The short measurements showed little speed difference and do not establish what caused the available-memory difference.
