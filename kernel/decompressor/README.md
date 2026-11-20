# SM-T700 compressed-kernel checkpoints

The sixth hardware trial uses two markers inside the Linux 6.12.118 ARM compressed kernel. TWRP can read either marker from its previous RAM console after a warm reset. The image reuses the [early ADB trial](../early-adb/README.md) ramdisk, USB setup and reserved-memory DTB. It keeps the S-BOOT TLB patch.

| Recovery console record | Stage reached |
| --- | --- |
| `KLIMT-HEAD6 A decompressor entry` | The compressed kernel reached the first hook, after saving the incoming boot parameters and switching to supervisor mode. |
| `KLIMT-HEAD6 B decompressed image` | The decompression call returned and the decompressor finished its cache cleanup. This hook precedes the jump into the expanded kernel. |

B replaces A. An authenticated USB shell reporting `6.12.118-klimt-head6` establishes userspace execution. Missing markers leave the result open. Each hook skips writing if the MMU or data cache is enabled, or if the existing vendor ring signature is absent. Reset retention also affects recovery readback.

## Build and verification

Use the SM-T700 ARMv7 ARM-state configuration and the exact 6.12.118 source from the early ADB trial. Set LOCALVERSION to `-klimt-head6`, disable PSTORE_RAM so that later kernel initialization leaves the marker in place, and keep the console's 2 MiB DT reservation. Apply [the hooks](head-hooks.patch) from the kernel source root with `patch -p1`. Generate `arch/arm/boot/compressed/klimt-markers.inc` with `python3 make-marker.py` from this directory, redirecting stdout to that file in the kernel tree. Use the early ADB packaging parameters and its original external ramdisk. The build-host checks compare both unpacked payloads with their inputs.

The stackless hooks use scratch registers and position-independent branches. The generator precomputes the 12-byte ring header, one 128-byte data block, and two 16-byte Reed–Solomon parity blocks. It commits the header after writing the content and parity. Both hooks use the vendor console base verified from the SM-T700 recovery kernel. [Generator](make-marker.py) · [TWRP console layout](../early-adb/README.md#persistent-console)

A separate ARM virt build used the generator's emulator option and the guest-specific reserved address. The host sampled RAM during a single boot and observed A followed by B. Both complete snapshots passed checks on the ring format, parity and untouched surrounding bytes. A second boot with a different starting signature left the full 2 MiB unchanged. Both guest runs reached an authenticated shell. These tests use an emulated USB controller.

## Hardware observation

TWRP verified a marker-free baseline console, the fifth-trial BOOT and the original RECOVERY on November 19, 2026. The sixth candidate measured 7,041,040 bytes, SHA-256 `8e6e4bd3652bacf84e4c30545df9e34d45ac46b1f82daa00a7369efe0d03edfb`. After writing and flushing BOOT, the complete partition matched `8d6eae98dbaca9ce0c294ff4d2b1f69040f26815aff54a02d811ce7124821e26` and RECOVERY retained its original hash.

At 22:00:24 PST the tablet rebooted normally from TWRP. The host saw the recovery USB device detach 1.7 seconds into a 185-second capture. The same physical port registered no device for the remaining 183 seconds. No ADB shell was observed.

TWRP had not reconnected by 22:05:30 PST. The diagnostic remains in BOOT. Hardware checkpoint readback requires a warm return to the existing recovery and an immediate capture of /proc/last_kmsg. The current observation establishes neither decompressor entry nor completion.
