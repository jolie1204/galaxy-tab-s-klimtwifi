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

TWRP reconnected at 22:15:41 PST. Two reads of the 6,740-byte previous console matched byte for byte. It contained recovery init messages, a reboot at about 535 seconds uptime, and a footer reporting 100 corrected bytes and zero unrecoverable blocks. Neither marker nor the trial kernel release was present. The complete BOOT and RECOVERY reads still matched their expected hashes. The running TWRP listed ram0 as an enabled console. This completes the sixth trial's readback. Whether the bootloader entered the 6.12 decompressor remains unresolved.

## Seventh trial with the vendor decompression address

The known-working Android 14 kernel expands to `0x20008000`. TWRP on this SM-T700 also reported RAM starting at `0x20000000` and kernel code starting at `0x20008000`. The seventh trial tests that destination in the sixth trial's image. [Vendor kernel verification](../README.md#known-working-android-14-kernel-on-sm-t700)

In the verified sixth zImage, offset `0xa8` masks the current PC to a 128 MiB boundary (`and r0,r0,#0xf8000000`). The trial substitutes one ARM-state instruction, `mov r0,#0x20000000`. The appended DTB starts exactly at the end of the zImage. Its magic takes the branch at `0xc4` to the instruction at `0xd8`, which adds `0x8000` into r4. In a Cortex-A15 emulator, the actual packaged payloads were both loaded at `0x41000000` and executed this address-selection sequence. At offset `0xdc`, r4 was `0x40008000` for trial six and `0x20008000` for trial seven. S-BOOT's actual zImage entry address on the tablet remains unknown. If it was already in the first 128 MiB of RAM, both paths would have selected the same destination.

The rebuilt BOOT container differs only in that instruction and the Android v0 header digest. Both checkpoints, the release string, DTB and early ADB ramdisk are identical to trial six. Before the test, the saved previous console contained neither checkpoint. The 7,041,040-byte image has SHA-256 `ac80ec4622eaed83ff8a473ff05e17f5818e5e1a3d73528df71c0b3cc7f0efb7`. After flushing, the complete BOOT matched `a53342c0f63f631744ee5c085ea13ff8495761a757ca7765218740a914054712` and the RECOVERY digest matched its pre-trial value.

The hardware boot request was issued at 22:40:00 PST on November 19. TWRP detached 0.4 s into a 185 s capture. The physical port and host kernel USB journal recorded no further enumeration during that interval, and the host obtained no early ADB shell. Recovery-console readback was pending at the end of the capture.

TWRP reconnected at 22:43:47 PST. Two reads of its 6,416-byte previous console matched. It contained neither decompressor checkpoint nor the trial release. BOOT still matched the seventh image plus its original partition tail, and RECOVERY matched its pre-trial digest. This closes the readback. Marker retention through the hardware reset remains uncalibrated.
