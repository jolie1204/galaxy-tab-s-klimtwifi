# Android 14 Home on the SM-T700

These trials extend the verified [removable-system Android boot](../drm-vkms-trial/README.md#real-android-display-services-from-a-removable-system-trial-46). The isolated Android system runs read-only from microSD with disposable `/data` in tmpfs. The kernel and first-stage init retain independent timed recovery paths. Acceptance requires Android's actual `sys.boot_completed=1`, a resumed Home launcher, a composed Android frame and active scanout on the built-in panel. The [panel trial](../../kernel/klimt-display/README.md) covers the hardware and low-brightness calibration.

## Trials 47 and 48

Trial 47 widened the kernel recovery timer to 690 seconds and the native fallback to 735 seconds to allow the first boot to compile the preinstalled APKs. It finished ART compilation, reported `StartAudioService` and `StartBiometricService`, then lost ActivityManager, PackageManager and WindowManager from its Binder checks. SurfaceFlinger remained registered. Android's `BiometricService` requires the framework-facing GateKeeper Binder service. The diagnostic was starting only the lower-level software Gatekeeper HAL.

For trial 48, [the RC patch](gatekeeper-crash.patch) imports and starts the installed `gatekeeperd` and `tombstoned` definitions, prepares `/data/tombstones` in RAM and enables the diagnostic log transport. The patch applies to the trial 47 diagnostic `klimt-property.rc`, which already starts the Binder, ART, networking and display services. Install [the polling script](home47-48-poll.sh) in the isolated system at `/system/bin/klimt-framework-poll.sh` with the same label as the stock Android shell. Run it with the existing trial service in the Android shell domain. The polling script reports actual Binder state and the boot-completed property. It attempts a native frame capture when the resolved and resumed Home components agree.

On the SM-T700, `gatekeeperd` and `tombstoned` started around uptime 31 seconds. Trial 48 progressed beyond biometric initialization, and all four framework and display services still responded at 298.847 seconds. The last reported Java phase was `Permission_callback_waiting-0`. The crash buffer confirmed a `system_server` fatal exception during default runtime permission grant or upgrade. Gatekeeper remained running, and Android boot completion and Home were unverified.

Both trials returned to fresh TWRP under the software timeout. The three retained-console reads agreed and the full trial BOOT and original RECOVERY matched their expected hashes. SYSTEM and EFS matched their prior payload hashes and both filesystems reported clean. The trial 48 BOOT partition SHA-256 was `11a8db896a773934cfed6cd0c97fcbef81f63155c7074daada9af91ad68954cc`.

## Trial 49

Add [the stock storage views](app-storage.patch) to the trial 48 init script and enable [built-in FUSE](app-storage.config) alongside the existing Android networking settings. `/mnt/expand` is required by the ordinary Android 14 Zygote app-data isolation path. The added init commands also prepare the runtime storage views, mount `fusectl`, and bind the default `/mnt/user/0` view onto `/storage`. ARM QEMU verified the bind and propagation, the FUSE device, and the expected security contexts with SELinux enforcing.

On the SM-T700, the removable ext4 root mounted read-only and the 2-nit display initialized. The trial finished first-boot package compilation and recorded ActivityManager, PackageManager, WindowManager and SurfaceFlinger responding at uptime 318.620 seconds. `isUserUnlocked-USER_SYSTEM` was the last sampled Java timing. The real `system_server` stopped before the next poll at 332.824 seconds. Toybox lacked the `awk` applet invoked by this trial's diagnostic filter, so its more detailed exception capture was unavailable. Android's standalone `/system/bin/awk` ran in the same ARM fixture and is used for the next measurement.

At uptime 709.603 seconds the kernel initiated recovery. TWRP returned in software, three retained-console reads matched, and original RECOVERY and complete BOOT passed readback. SYSTEM and EFS retained their payload hashes and reported clean. The trial 49 BOOT partition SHA-256 was `b90cfaccb87cb7f2c2b10d77b2e6153e8d4d36613db022e04996050b33253cb0`. Boot completion and Home remain unverified.

## Trial 50

Use [stock user storage ownership](vold-user-storage.patch) and let vold prepare the user-specific CE and DE directories. The Android `/system/bin/awk` corrected the log collection pipeline. The physical framework reached phase 550 and the user unlock checks. `system_server` then exited, and the crash capture included a native SIGSEGV in Google Setup Wizard. The PC-side log preserves the full diagnostic timing and distinguishes the app crash from the framework exit.

On ARM QEMU the permission-controller process started and bound before an application ANR and framework watchdog abort. Android Settings logged repeated ashmem creation failures. The test Android system reported `ro.treble.enabled=false`, so the platform libcutils uses `/dev/ashmem` even though the test kernel already supports memfd. This determined the next kernel compatibility test.

The kernel timer returned the tablet to TWRP in software. A serial-scoped libusb ADB server reconnected to the fresh recovery on the original port after the default host USB backend missed the new instance. Three retained-console reads matched. The full BOOT partition SHA-256 was `fe557a1198fb91fcb9aa8ae60f94b9cbf5eacbb91983dccf136df492c95a3dd2`. Original RECOVERY and both clean SYSTEM and EFS payloads verified. The Android init reboot request had tried to locate a stock OTA partition path during the diagnostic. Subsequent tests use the existing kernel recovery timer and first-stage fallback.

Trial 51 adds the [verified ARM ashmem compatibility driver](../../kernel/klimt-ashmem/README.md) for the non-Treble userspace. The physical Home acceptance test remains in progress.
