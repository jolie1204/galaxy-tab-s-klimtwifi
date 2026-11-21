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

## Trial 51 and the kernel configuration readback

Trial 51 booted the [ARM ashmem compatibility driver](../../kernel/klimt-ashmem/README.md) for the non-Treble Android system. The retained real-device console confirmed `ashmem: initialized` at 2.460 seconds. After the framework reached the user unlock checks, the native `system_server` tombstone reported SIGABRT with `Kernel configs could not be fetched`. A separate SystemUI exception reported a null bitmap. TWRP returned in software, with three identical retained-console reads, original RECOVERY, matching clean SYSTEM and EFS payloads, and full BOOT SHA-256 `db4f96ad55b442bef814f20f2200e485cafaf55604cf672893cdd631b69d3b42`.

Android's `Debug.isVmapStack()` calls libvintf to load `/proc/config.gz` for memory accounting. Apply [the kernel settings](android-kernel-report.config) so Linux exports its actual built-in configuration. The stock Android policy labels the interface `config_gz` and already allows `system_server` to read it. The ARM guest read and decompressed `/proc/config.gz` while enforcing and verified `IKCONFIG`, `IKCONFIG_PROC`, `ASHMEM` and `FUSE_FS` from the running kernel. The 8,153,104-byte trial 52 BOOT passed the three complete image replays.

## Trials 52 and 53

The SM-T700 with kernel configuration readback kept ActivityManager, PackageManager, WindowManager and SurfaceFlinger registered through 353.443 seconds. Android later stopped `system_server` after a 63-second main-thread stall. The full ARM watchdog stack identified `GnssNative.native_class_init_once` during boot phase 600. The physical trial independently logged 48 requests to activate the declared and unavailable GNSS HIDL service. TWRP returned in software and the complete trial 52 BOOT, original RECOVERY and clean SYSTEM and EFS payloads verified.

[The diagnostic manifest patch](omit-unavailable-gnss.patch) removes the unavailable GNSS HAL and the `android.hardware.location.gps` feature from the isolated removable system. The regular Android location feature and the other HAL declarations are preserved. Apply the patch from the root of the private ext4 system image with `patch --fuzz=0 -p1`. Keep the original declaration and feature file for a future working GNSS port.

The ARM guest reported general location present and GPS absent and resumed Google Setup Wizard, which establishes progress beyond the GNSS initialization point. The SM-T700 completed the GNSS comparison without the prior repeated HAL activation requests. Its `PermissionPolicyService` then reported a failure to grant or upgrade runtime permissions during phase 550. The device returned to TWRP in software. Original RECOVERY, clean SYSTEM and EFS payloads, and the three retained logs verified. The full trial 53 BOOT SHA-256 was `761f7839c509dbab7ceb685b6e95b67395d0eb92da0f3ac89bb78b0aa4269bcd`. Android boot completion and the real Home launcher remain unverified.

## Trial 54

The deliberately single-CPU diagnostic applied the Android framework timeout multiplier of 3 before Zygote. It passed the prior permission-service shutdown time. In the first run, Android RescueParty requested a warm reboot, and the same diagnostic restarted. The second run reached a framework UI-thread watchdog timeout after 193 seconds. The SystemUI native diagnostic identified `drawRenderNode called on a context with no surface`, which comes from the Skia OpenGL pipeline when no EGL surface is available. The second boot timed out to TWRP automatically. The complete BOOT read back as `c62ce5b5f3beb5753c9aacd5c94c4398ca66e5ff347bb0c9ea2e2a9448f04fea`. Original RECOVERY, clean SYSTEM and EFS payloads, and the three retained log copies verified.

## Trial 55

[The diagnostic patch](skia-vulkan-rescue.patch) selects Android's `skiavk` HWUI pipeline before Zygote and captures SystemUI exceptions while the Java framework still responds. It sets the stock RescueParty control property in the isolated RAM environment. [The device-tree overlay](rescue-to-recovery.dts) also maps an explicit `RescueParty` reboot to the known Samsung recovery values. The two independent deadline-based recovery paths remain available. The staged device tree differed only by the two reboot-mode aliases. Shell syntax, exact patch reconstruction and all three complete boot-image replays passed.

The SM-T700 reported the Vulkan renderer setting, the RescueParty property and the calibrated panel's DCS power mode `0x9c`. ActivityManager, PackageManager, WindowManager and SurfaceFlinger remained registered at 473.029 seconds. Android then killed Zygote at 482.202 seconds. The focused log identified a `TimeoutException` from `PermissionControllerManager` while `PermissionPolicyService` was granting or upgrading user 0's default runtime permissions during framework boot phase 550. A SystemUI `DeadSystemException` followed the framework exit. The earlier native OpenGL surface abort did not appear in the retained log. Android's boot-completed property was unset and normal Home rendering remained unverified.

The kernel requested recovery at 709.603 seconds. TWRP returned on the original port in software without an intervening Android reboot. Three retained console reads, the full BOOT and original RECOVERY matched. The full BOOT partition SHA-256 was `79be6cbcc0d4f630c5a340fe99f727b1b43724753bf7545b8b690bcc8a8b8dcc`. SYSTEM and EFS payload hashes matched and both filesystems reported clean. The isolated system and its disposable data remain suitable for the next permission-controller diagnosis.

## Trial 56

[The first-boot ART comparison](defer-first-boot-dexopt.patch) applies Android's supported `pm.dexopt.first-boot=skip` filter before Zygote in the isolated image. The diagnostic keeps the ordinary permission code and enforcing policy. It records the permission-controller process and activity-manager events alongside user and window readiness. Exact patch reconstruction, shell syntax and all three complete boot-image replays passed.

On the SM-T700, the property was active at 33.944 seconds. The permission phase appeared at 105.319 seconds and the PermissionController process attached by 111.535 seconds. The framework progressed to `StartSystemUI` at 136.484 seconds. This passed the permission timeout seen in trial 55. SystemUI later aborted repeatedly at `queueProps[i].queueCount < kRequestedQueueCount`. Android 14 HWUI requires separate Vulkan queues for rendering and hardware-buffer upload. The SwiftShader driver advertised one. A 191-second display-thread watchdog timeout stopped the framework before Android finished booting.

The kernel requested recovery at 709.609 seconds. Fresh TWRP and three matching console copies verified. Complete BOOT SHA-256 was `83487ab9c8ab1419e66ab8f2eb5564c14f53b711c3a80756c2ca31b561093fc1`. Original RECOVERY and both clean internal SYSTEM and EFS payloads also matched. The native rendering path remained the next physical acceptance point.

## Trial 57

Apply [the two-queue patch](two-swiftshader-queues.patch) in `external/swiftshader` and [the fence capability patch](hwui-vulkan-fence-capabilities.patch) in `frameworks/base`. SwiftShader constructs a separate `Queue` and worker thread for each requested queue. HWUI checks whether Linux `SYNC_FD` semaphore handles support import and export. When a driver lacks either capability, HWUI waits for work to finish before handing off the buffer. Rebuild `vulkan.pastel` and `libhwui` for `klimtwifi` and keep the vendor and system SwiftShader paths on the same labeled inode. The ARM libraries built with the same dynamic exports and dependencies. Both patches reproduced their built source with zero fuzz.

Compile [the queue probe](vulkan-queue-probe.c) with `armv7a-linux-androideabi34-clang -O2 -Wall -Wextra -Werror -lvulkan -llog`. The four-CPU Android ARM control verified distinct queues, a semaphore-ordered 4096-byte transfer and the absence of `SYNC_FD` capability. The single-CPU ARM control returned `VK_TIMEOUT` waiting for the transfer fence. The physical standalone probe also failed its bounded check. These controls do not establish performance or complete queue compatibility on the tablet.

The SM-T700 reused the exact trial 56 BOOT. It reached `ActivityManagerStartApps` at 124.833 seconds. WindowManager confirmed its service readiness, boot state and keyguard draw at 130.366 seconds. Android's `sys.boot_completed` remained unset. The earlier queue-count assertion was absent from the retained log. The live SystemUI trace identified a null bitmap in `KeyButtonDrawable.draw`. HWUI reported buffer dequeue failure and could not allocate its scratch buffer. SurfaceFlinger independently reported failure to allocate a screen-capture buffer. The framework watchdog stopped the system after a 189-second UI-thread stall. These traces point to graphics buffer allocation as the next blocker.

The scheduled kernel recovery returned to fresh TWRP. The original RECOVERY, the unchanged complete BOOT, three retained log reads and both clean internal filesystem payloads verified.

## Trial 58

Apply [the Exynos primary-node patch](exynos-primary-node.patch) in `external/minigbm` on top of the [Exynos dumb-buffer backend](../drm-vkms-trial/exynos-minigbm.patch). Linux restricts `CREATE_DUMB` and `MAP_DUMB` to primary DRM nodes. The allocator rejects the Exynos render node and releases any DRM master role it acquires on the primary node so the hardware composer can control scanout. The physical removable-system audit confirmed the installed library already contained `backend_exynos`. The replacement retained its complete dynamic exports and dependencies. Exact source reconstruction and the ARM build passed.

Build [the Android hardware-buffer probe](gralloc-probe.c) using `armv7a-linux-androideabi34-clang -std=c11 -O2 -Wall -Wextra -Werror -landroid`. The enforcing ARM control allocated and mapped a 1×1 scratch buffer, then allocated a 2560×1600 compositor buffer. The SM-T700 reused the exact prior BOOT. On that run, kernel audit showed the graphics allocator supplying reserved `memfd` regions and rejecting their transfer to `shell`, `surfaceflinger` and `platform_app` under the original SELinux policy. Both physical buffer requests reported error 5. Google Setup Wizard's `WelcomeActivity` later resumed while `sys.boot_completed` remained unset. SystemUI again reported a null bitmap after `AHardwareBuffer_allocate()` failed. The framework watchdog stopped the UI thread after 189 seconds.

The same boot recorded a Bluetooth process abort because the stock `/data/misc/bluetooth/logs` path was absent from the minimal diagnostic init. The next trial restores those standard directories in its disposable RAM data.

The software kernel deadline returned the tablet to fresh TWRP at 04:41:22 PST. Three retained console reads, the complete reused BOOT, original RECOVERY and both clean internal SYSTEM and EFS payloads verified. The previous physical allocator library and the tested removable file hashes were retained for rollback.

## Trial 59

[The metadata rules](gralloc-metadata.cil) allow `platform_app`, `surfaceflinger` and `shell` to read, write, inspect and map the graphics allocator's reserved metadata file type. Its MLS attribute permits the category-aware handoff for platform apps. The original policy and a recompilation control yielded the same effective allow rules. The patched policy introduced exactly the twelve expected individual permissions and one file-type attribute membership. Its SELinux file transitions, MLS constraints, enforcement mode and permissive domains were verified against the control.

[The Bluetooth init patch](bluetooth-log-directories.patch) runs Android's stock directory creation on the diagnostic RAM data. It restores the paths expected by Bluetooth's snoop logger. Physical logs showed successful directory initialization and later requests for the unavailable Bluetooth HCI service. Hardware Bluetooth functionality remains open.

The physical Linux kernel committed the exact new policy while enforcing. The 1×1 scratch buffer allocated and mapped at 41.919 seconds. The 2560×1600 compositor buffer allocated at 43.664 seconds. The native probe returned 0. The later physical audit identified two more receivers of the same metadata type. The hardware composer received navigation bar and framebuffer surface descriptors from SurfaceFlinger. `system_server` received screenshot metadata. Both recipients reported access denials. A 189-second Android display-thread watchdog stopped the framework before the Home screen appeared.

The kernel deadline returned the tablet to fresh TWRP at 04:56:31 PST. The reused complete BOOT, original RECOVERY and both clean internal payloads verified. Three retained console copies matched. Keep the original and trial-59 policies for rollback. The next diagnostic tests only the two additional audited receivers and adds physical display observations.

## Trial 60

[The further metadata rules](graphics-metadata-receivers.cil) extend the trial 59 policy to the hardware composer and `system_server`. Both appeared in the real kernel audit. The recompilation comparison showed the two expected file rules. The SM-T700 loaded the exact compiled policy while enforcing, repeated both successful buffer probes, and recorded no further denials for that metadata type during the run.

SurfaceFlinger identified hardware display 0 and read `powerMode=On` at 200.182 and 258.308 seconds. WindowManager reported readiness and keyguard draw completion. A Home activity and composed Home pixels were still unavailable. The expanded watchdog trace identified the stalled Android display thread's call to `UsbService.Lifecycle.onBootPhase`. The framework's boot-completion stage was waiting for the earlier USB initialization future. Native logs showed repeated requests for the declared `android.hardware.usb@1.0::IUsb/default` interface. The removable system contains LineageOS's basic USB port HAL but the minimal diagnostic init had skipped its service definition. The next hardware run starts the installed HAL.

Fresh TWRP returned by software at 05:11:42 PST. Complete BOOT and original RECOVERY matched, and SYSTEM and EFS payloads and filesystem checks passed. Three retained console reads matched. A separate host-only GL comparison was prepared. The exact physical watchdog stack established the installed USB service as the next correction.

## Trial 62

[The init patch](start-installed-usb-hal.patch) imports and starts the existing LineageOS USB 1.3 Basic HAL in the removable Android 14 system. The enforcing ARM comparison registered the native HIDL service and the Android framework's `usb` Binder service. The patch reproduced the exact staged init file with zero fuzz. The updated removable filesystem passed `e2fsck -fn` and SHA-256 readback.

The SM-T700 reused the complete trial 56 BOOT. The native USB port HAL started at 35.332 seconds, the Android framework's `usb` Binder service registered at 118.123 seconds, and `sys.boot_completed=1` appeared at 156.554 seconds with ActivityManager, PackageManager, WindowManager and SurfaceFlinger available. Both native buffer probes passed. The resumed activity at 167.467 seconds and again at 211.653 seconds was Google Setup Wizard's `WelcomeActivity`. Trebuchet and a composed Home frame remained unverified.

The retained audit found the same graphics metadata memfd reaching `priv_app` without its matching file rule. The late system-server watchdog captured `ColorFade.draw` blocked at `EGL14.eglSwapBuffers` on the PowerManagerService handler. The dedicated Linux deadline requested recovery at 709.603 seconds. TWRP returned by software on the original USB port at 05:32:01 PST. Three retained console reads, complete BOOT, original RECOVERY and both clean internal SYSTEM and EFS payloads verified. Keep the previous removable init and polling files for rollback.

## Trial 63

[The polling script](home63-poll.sh) verifies an unlocked user 0 and tmpfs `/data` before touching the disposable profile. It sets and reads Android's standard provisioning flags, keeps the test display awake within the software-recovery window, and requests the regular HOME intent scoped to Trebuchet's package. The frame sampler targets SurfaceFling's internal physical display and requires actual foreground focus, display power On and a nonblack 2560×1600 frame. [The additional SELinux rule](priv-app-graphics-metadata.cil) matches the `priv_app` recipient observed in trial 62. Decoded policies differed by this one graphics allocator file rule. The host guard, user-state, launcher and display cases passed. The enforcing ARM control verified real boot completion and both native Android buffer probes.

On the SM-T700, `sys.boot_completed=1` appeared at 195.870 seconds. The screen-timeout readback was 900000 milliseconds at 207.839 seconds. The diagnostic read back both RAM setup flags as 1 at 391.782 seconds. ActivityTaskManager reported `com.android.launcher3/.uioverrides.QuickstepLauncher` resumed at 439.500 seconds. The scope-specific graphics metadata denials were absent. The WindowManager focus and SurfaceFlinger display queries were unreported, so this run established the first resumed Home activity. A focused, composed frame on the physical panel remains to be verified.

The kernel's diagnostic deadline requested recovery at 709.882 seconds and TWRP returned by software at 05:47:34 PST. Three retained-console copies, the complete existing BOOT, original RECOVERY and both clean SYSTEM and EFS payloads verified. The previous removable policy and polling script remain available for rollback.

## Trial 64

A vendor runtime resource overlay targeted `android:bool/config_displayColorFadeDisabled`. Host idmap2, using the exact fixture framework resources and the vendor policy, mapped that flag to true. The enforcing Android ARM build discovered the overlay package, reported `STATE_NO_IDMAP` and left DisplayPowerController's ColorFade enabled. On the SM-T700 the framework completed boot at 173.107 seconds and the live resource lookup returned false at 186.693 seconds.

Both disposable initial-setup flags read back as 1 at 201.709 seconds. The real Trebuchet activity resumed at 532.704 seconds, with the internal HWC display 0 reporting `powerMode=On` at 546.669 seconds. The later 686.168-second sample again identified Trebuchet. The polling command selected WindowManager's windows dump. Android 14's `mCurrentFocus` field lives in its display contents dump. The incorrect query kept the capture guard from reaching `screencap`.

The kernel requested recovery at 710.220 seconds. Fresh TWRP returned at 06:03:21 PST. The same complete BOOT and original RECOVERY readbacks matched.

## Trial 65

The unused runtime overlay was removed from the removable test image. [The exact resource patcher](set-colorfade-resource.py) accepts the audited original framework APK and changes one Boolean `config_displayColorFadeDisabled` value. Align the resulting APK with Android's `zipalign` and sign it with the matching AOSP platform test certificate. The full decoded resource table changed only from false to true on that entry. Every unsigned APK entry matched except those four data bytes, and the original Android signing-certificate digest verified. The enforcing ARM runtime reported `mColorFadeEnabled=false`.

[The polling script](home65-poll.sh) obtains the real `mCurrentFocus` through `dumpsys window displays`, targets the internal SurfaceFlinger display for capture and accepts either 1600×2560 or 2560×1600. It verifies user 0 and tmpfs `/data` before updating the disposable profile. The revised focus query was separately checked with live WindowManager in the ARM guest. The host logic cases passed for both orientations and the user, focus and display gates.

The SM-T700 reported `sys.boot_completed=1` at 198.995 seconds. The Android resource lookup confirmed `COLORFADE_DISABLED true` at 211.944 seconds. The real Trebuchet Home resumed at 267.623 seconds and the internal display was On at 298.020 seconds. This run observed the launcher roughly 265 seconds earlier than trial 64. A focused Home window and a composed Home frame were still unavailable. The same real launcher was sampled again just before Linux requested software recovery at 709.892 seconds.

TWRP returned automatically at 06:18:46 PST. Three console reads, the complete reused BOOT, original RECOVERY and the clean internal SYSTEM and EFS payloads matched. The retained removable framework and polling script provide the diagnostic rollback. The next comparison changes the app rendering path to Skia GL.

## Trial 66

The packaged hardware comparison selected `debug.hwui.renderer=skiagl` before Zygote, using the accepted trial 65 removable framework and SELinux policy. [The observation patch](frame-before-focus.patch) also records the raw focused-window query, captures a diagnostic-only frame when display power is On and focus is pending, and reports a Home frame separately only after matching real launcher focus. Host reconstruction and targeted shell guard checks passed. The enforcing ARM guest confirmed Skia GL, boot completion, inactive ColorFade, working USB and both graphics buffer probes. An isolated 15-second native capture there timed out.

The physical SM-T700 selected Skia GL at 33.924 seconds and reported Android boot completion at 175.249 seconds. Both RAM provisioning settings read back as 1. The separately installed LineageOS Setup Wizard resumed at 224.153 seconds after stopping the Google wizard. Android's default-activity resolver was foreground at 307.426 seconds. The late activity call returned a broken pipe and Zygote received SIGKILL at 437.012 seconds. SurfaceFlinger remained listed by Binder. The renderer comparison did not establish a focused Home window or Android Home frame.

Linux requested recovery at 709.603 seconds. Fresh TWRP returned by software at 06:33:57 PST with the exact complete Skia GL BOOT and original RECOVERY. The next comparison uses the previously verified Vulkan BOOT to test both observed setup flows and Android's ordinary default HOME role in RAM.

## Trial 67

[The default HOME patch](default-home-role.patch) applies after [the frame and focus observation patch](frame-before-focus.patch). It verifies user 0 and tmpfs `/data` before making any profile changes. With both standard provisioning settings read back as 1, it disables only the two observed setup wizards for the disposable user, sets Trebuchet as Android's HOME role and verifies the normal unqualified intent resolves to it. Both APKs stay on the removable system. The exact previously verified Vulkan BOOT was restored and saved separately from the comparison GL BOOT. The enforcing ARM baseline repeated the native USB and graphics allocation checks.

On the real SM-T700, Android finished booting at 172.403 seconds. Both wizard package states and the preferred Trebuchet default were verified by 224.822 seconds. The actual Home activity resumed at 227.153 seconds. At first its window focus was null. WindowManager later identified Trebuchet as the focused window at 471.614 and 634.790 seconds. SurfaceFlinger confirmed physical HWC display 0 and `powerMode=On` in both samples. Both full-resolution native screen captures returned failure while that Home session was active. The independent pixel readback remained outstanding.

Linux requested software recovery at 709.966 seconds. Existing TWRP returned at 06:49:31 PST. Complete Vulkan BOOT and original RECOVERY, three retained console reads and the clean SYSTEM and EFS payloads verified. The optional software-Canvas launcher stayed off the device because the installed launcher acquired real focus. The next comparison keeps the verified Home configuration and reduces only the requested dimensions of the diagnostic screen capture.

## Trial 69

[The AOSP screencap patch](quarter-scale-screencap.patch) adds the optional `-q` flag in `frameworks/base`. It requests one quarter of each measured logical display dimension through Android's regular physical-display capture. Compile the platform ARM executable with the same linker dependencies and interpreter as the original. [The matching polling patch](quarter-scale-home-frame.patch) applies after trial 67 and keeps the same setup and render configuration. The raw frame validator requires a complete RGBA or RGBX payload and counts actual RGB values. Both patch replays, shell syntax, enforcing ARM USB and buffer allocation, and positive and negative frame guard cases passed.

The SM-T700 used the full verified Linux 6.12.118 BOOT and the read-only removable Android 14 system. SELinux remained enforcing and Android app data lived in RAM. The live ACM report captured the following observations. The final five observations came from one polling cycle.

| Kernel uptime | Verified on the tablet |
| --- | --- |
| 176.509 s | `sys.boot_completed=1` with activity, package, window and SurfaceFlinger Binder services reported |
| 534.800 s | Trebuchet `com.android.launcher3/.uioverrides.QuickstepLauncher` resumed |
| 535.356 s | WindowManager focus matched the same Trebuchet component |
| 536.203 s | SurfaceFlinger reported internal HWC display 0 |
| 536.293 s | Internal display `powerMode=On` |
| 544.429 s | Native `screencap` returned a 400×640 RGBA frame with 256,000 nonblack RGB pixels |

[The evidence checker](verify-home-evidence.py) requires boot completion, matching launcher and window focus, physical HWC display On, and a nonblack frame in the same polling cycle. The first reduced capture returned successfully but did not pass the frame verification step. The repeat produced the accepted physical Home frame. The measurement is a quarter-scale capture of the portrait display. The diagnostic retains the pixel count and discards the raw frame after validation. An installable firmware build, sustained performance checks and peripheral testing remain open.

The kernel requested recovery at 709.612 seconds. The existing TWRP returned by software at 07:04:56 PST. The verification markers match in the live USB and retained kernel consoles. The three retained reads match, the complete BOOT and original RECOVERY match, and the internal SYSTEM and EFS payloads are unchanged beyond their ext4 mount metadata block. Both filesystems reported clean. Keep the original installed Android BOOT and the previous removable screenshot tool and poll for rollback. The tablet finished the test in TWRP.

## Direct camera verification of the 105-nit removable Home profile

The Trial 69 removable system and disposable RAM profile were rerun with the [105-nit S6E3HA1 calibration](../../kernel/klimt-display/README.md). The full microSD image was verified before boot. The expanded Linux 6.12.118 image changed only the 40-byte panel calibration vector. Both decompression stages passed all three ARM handoff cases. The 8 MiB test BOOT SHA-256 was `2bfa4c983f1c1b0c40d56ea3d57e40c758a1b9ea61eb82bad84745d24e66214b`.

On the SM-T700, Android reported boot completion at 171.577 seconds. The external camera showed normal Trebuchet Home on the built-in panel at 12:28:27 PST, with the search widget and clock visible later. Android independently logged the real Trebuchet activity at 286.259 seconds, matching WindowManager focus at 334.675 seconds and internal HWC display 0 On. The camera showed the Home screen for approximately five consecutive minutes and again after a brief black interval. The first native capture timed out during the visible interval. A later request returned while the main area was black and did not pass the frame acceptance check. The synchronized optical and native frame check remained open after this run.

The kernel recovery timer fired at 709.610 seconds. TWRP returned automatically at 12:36:39 PST with a fresh boot ID, and complete BOOT and original RECOVERY digests matched. The verified normal Home applied to the read-only microSD and RAM run. The installed internal profile still requires a working setup or Home window. Raw camera and device logs remain private.

## Paired Android composer and camera evidence

Four earlier native screenshot requests reached their timeout while the camera still showed Home. A temporary 800×1280 Android display-size override made the UI cover only part of the real panel. The paired trial restored the original Android display size, kept the verified 105-nit BOOT and used Android's existing physical composer to capture the committed buffer.

Apply [the composer sampling patch](hwc-scanout-sample.patch) in `external/drm_hwcomposer` and rebuild the ARM `hwcomposer.drm_minigbm` vendor module. Keep this patch in the disposable read-only microSD profile with RAM app data. The module checks the built-in connector, full-size linear RGB buffer, acquire fence and successful DRM commit. Its bounded background reader samples 8×12 RGB points from the panel's 1600×2560 buffer. The build preserved the original SONAME, shared dependencies and defined dynamic symbols.

Apply [the observer patch](hwc-scanout-observer.patch) after the Trial 69 observer. It starts under the original RAM mount guard and forwards the composer sample and kernel uptime through the diagnostic console. Run [the host evidence reader](verify-scanout-evidence.py) against the private camera and ACM evidence directory. It decodes the two sample halves and selects camera frames on both sides of the estimated capture time. Inspect the selected photos for recognisable Home content.

Android reported boot complete at 172.209 seconds, Trebuchet resumed at 239.941 seconds, and WindowManager focused it at 246.404 seconds. The internal HWC display reported On. The initial sample at 220.370 seconds was black. The samples at 373.255 and 397.406 seconds both matched framebuffer 76, one scanout plane, the 1600×2560 buffer, and 96 of 96 nonblack RGB samples. The surrounding camera frames at approximately 13:16:53 and 13:17:18 PST showed the Trebuchet wallpaper and app dock. The later frames also showed the search widget. These pair the actual built-in panel with the Android composition in the same boot. At 566.469 seconds another nonblack Android buffer coincided with a black main area in the camera. Sustained output remains open. The test used only the read-only removable system and disposable RAM profile. The internal installation's setup and Home remain open.

The kernel requested recovery at 710.280 seconds. Original TWRP reconnected at 13:22:52 PST with a fresh boot ID. Full BOOT and RECOVERY matched their preboot digests, and two identical retained console copies independently contained both accepted frame markers.
