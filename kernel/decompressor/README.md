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

TWRP reconnected at 22:43:47 PST. Two reads of its 6,416-byte previous console matched. It contained neither decompressor checkpoint nor the trial release. BOOT still matched the seventh image plus its original partition tail, and RECOVERY matched its pre-trial digest. This closes the readback. The subsequent controls below tested the recovery console across two reset paths.

## Recovery console retention control

On November 19, TWRP wrote a fresh priority-2 message to `/dev/kmsg`. The token was absent from the previous console and appeared exactly once in the live kernel log. At 22:48:29 PST the host requested one reboot directly into recovery. At 22:48:52 the same tablet returned in TWRP with a different kernel boot ID. Two reads of the 6,928-byte `/proc/last_kmsg` were byte-identical and contained the token exactly once. The console footer reported no errors. The complete BOOT and RECOVERY hashes matched their pre-reboot values, with the seventh diagnostic still in BOOT.

For the button-reset control, TWRP wrote a second, distinct priority-2 message at 22:50:59 PST. It appeared once in the live kernel log and was absent from the preceding boot’s saved console. The operator used the hardware buttons to return to TWRP. The host saw USB disconnect at 22:51:56 and verified a new boot ID at 22:52:19. Two reads of the 6,255-byte previous console matched byte for byte and contained the new message exactly once. The ECC footer reported two corrected bytes and zero unrecoverable blocks. The complete BOOT and RECOVERY digests matched their pre-test values.

A message written by the running recovery kernel survived both the orderly reboot and the tested physical button reset. A stalled trial starts the reset in a different machine state. The early 6.12 hooks write the RAM ring directly and may skip the write under their MMU, cache or ring-signature guards. Whether either hook executed on the tablet remains open.

## Eighth trial with the vendor 3.4 decompressor

This control uses the October LineageOS 21 BOOT. Its expanded 14,987,772-byte kernel matches the working Android kernel byte for byte. A branch at zImage offset `0x5c` enters a stackless stub appended to the compressed payload, after the vendor code saves the boot arguments and sets supervisor mode. The stub runs the entry macro from [the marker generator](make-marker.py) and returns to the vendor instruction at offset `0x60`. The original compressed stream stays intact. The zImage length field includes the stub. The [packager](vendor-control/package.py) checks the original BOOT hash and reproduces the complete original container with its original kernel and ramdisk before building the control.

For a build, choose a fresh hexadecimal run ID. Invoke the generator's `macro('mark_vendor', 'KLIMT-VENDOR-ENTRY ' + run_id, 0x40000000)` in an ARM-state assembly file after selecting `.syntax unified`, `.arm` and `.text`. Append a `mark_vendor` invocation, assemble for ARMv7-A and extract the raw text section as the stub. The packager supplies the return branch. The RAM-only newc archive contains the [init script](vendor-control/init), a static ARM BusyBox at `/bin/busybox`, the run ID with a newline at `/run-id`, and [the recovery reboot helper](vendor-control/restart.S) at `/bin/restart-recovery` (static ARMv7, entry `_start`, no libc). Include directories `/bin`, `/dev` and `/proc` and character nodes `/dev/console` (5,1), `/dev/null` (1,3) and `/dev/kmsg` (1,11). Compress the archive with gzip and run `python3 package.py original.img stub.bin ramdisk.gz vendor-control.img`.

The test seeds the running recovery console with a distinct run-specific message before a normal reboot. Vendor PID 1 checks its own previous console for the early marker and the recovery seed, writes the result to its current kernel log, then requests a reboot to recovery. If the tablet stalls earlier, the returning TWRP may still recover the early marker directly. A userspace report and a direct marker have different meanings.

In Cortex-A15 emulation, the packaged zImage reached the original fixed-address load with the expected boot arguments, ring content and parity. With an invalid signature it left the entire 2 MiB test region intact. A separate emulated Linux boot ran the packaged PID 1 and issued the recovery reboot command. The device trial image measured 5,918,736 bytes, SHA-256 `f5cad789da3b5df3b5994d37f48d66d9db65e7aeeebf6761dbf697bf69abe758`.

The host requested the hardware boot at 22:58:32 PST on November 19. Vendor PID 1 reported `3.4.113-Deathly-1.0+` at 3.76 seconds of kernel uptime. Its 6,699-byte previous console contained the recovery seed and no exact early-marker line. The vendor reboot handler received `recovery` at about 64 seconds of uptime. At 23:00:05 PST the host verified a fresh TWRP boot. Two reads of its 3,594-byte previous console matched and contained the vendor PID 1 report and reboot request. No button input was needed. The host restored the exact 8 MiB seventh-trial BOOT; its full digest and the original RECOVERY digest matched after flushing.

The missing marker now has a confirmed example on a kernel that reached userspace on this tablet. It cannot establish failure to enter the 6.12 kernel. The retained seed is consistent with the early write being skipped. The MMU/cache flags, ring signature at hook time, and which hook instructions executed on hardware were not measured. An independent record of the entry conditions on the vendor path would distinguish these cases.

## Ninth trial measuring the entry guards

The SM-T700 vendor control samples the ARMv7 system control register (SCTLR) before the vendor decompressor enables caches. It reads the console signature at physical `0x40000000` only if both bit 0 (MMU) and bit 2 (data cache) are clear. The original guarded marker also runs. The probe carries both samples in registers across the vendor Cortex-A7/A15 cache-enable routine and stores them with the pre-relocation zImage base inside the decompressor's relocation span. After decompression and cache shutdown, it appends three hexadecimal words to the expanded kernel's default command line as `klimtdiag=SCTLR,signature,base`. If either guard bit is set, zero in the signature field means the read was skipped. Vendor PID 1 relays only this field to the recovery console. [ARM instrumentation](vendor-control/guard9-head.patch) · [RAM init](vendor-control/init)

The patch targets the verified vendor source revision from the [Android 14 kernel comparison](../README.md#known-working-android-14-kernel-on-sm-t700). Generate `guard9-marker.inc` with the eighth-trial macro named `mark_vendor`; the expanded Image must have SHA-256 `2b2fe646332feaedf877ffc5702902203468224b2869daa6afbcaba1709241df`. The host verified the default command-line array at Image offset `0xc3768c`, its terminator at `0xc376cd` and sufficient empty space in the 1,024-byte array. The wrapper contains the original OTA LZMA stream and original expanded-size word.

In Cortex-A15 emulation the wrapper took three paths: matching signature, mismatching signature with overlapping input/output requiring relocation, and enabled data cache with a skipped signature read. At kernel handoff the reported words matched each case, the boot arguments were preserved, and the entire expanded Image matched the OTA apart from the expected command-line suffix. For this emulator only, the debugger redirected the output address into guest RAM and bypassed the vendor UART print routine. A separate RAM-init boot verified field collection and the recovery reboot request.

On the SM-T700 the host requested a normal reboot at 23:09:10 PST. Vendor PID 1 reported `ENTRY 00c5187f,00000000,20008000` at 3.80 seconds uptime. Both SCTLR.M and SCTLR.C were set. The signature field was a skipped-read sentinel, and the guard also skipped its write. The vendor previous console contained the TWRP seed and no entry marker. The decompressor ran at a pre-relocation base of `0x20008000` as observed by the executing code; this probe did not walk the bootloader's page tables to translate that address. TWRP returned automatically at 23:10:43 PST and yielded two matching console reads. The whole seventh BOOT and original RECOVERY hashes were verified after restoring BOOT.

This explains the absent entry marker on the working vendor control. It does not locate the stopping point of the 6.12 trials. The measured vendor entry state differs from the documented ARM Linux handoff, which expects the MMU and data cache disabled. Comparing the first page-table and cache transition in the two decompressors is now a targeted 6.12 experiment. [ARM Linux boot protocol](https://docs.kernel.org/arch/arm/booting.html)

## Replaying the tenth image at the vendor entry address

After the [tenth hardware trial](../early-adb/README.md#tenth-sm-t700-hardware-trial), the host replayed the original vendor zImage and the kernel payload extracted from the exact tenth BOOT image. Both ran on a QEMU Cortex-A15 with RAM at the addresses used by the decompressor. A small loader set up an identity-mapped ARM section table at `0x26000000`, set SCTLR to the vendor control's measured `0x00c5187f` and entered each unchanged payload at `0x20008000`. The loader supplied synthetic ATAGs. The original S-BOOT translation table and the Exynos peripherals were outside the model.

| Checkpoint in emulation | Original vendor 3.4 | Tenth 6.12.118 |
| --- | --- | --- |
| Return from first cache setup | PC `0x20008068` | PC `0x200080f8` |
| Destination in r4 and new TTBR0 | `0x20008000`, `0x20004000` | `0x20008000`, `0x20004000` |
| SCTLR and domain access control register after setup | `0x00c5587f`, `0xffffffff` | `0x00c5587d`, `0xfffffffd` |
| Expanded kernel entry | Reached `0x20008000` with the UART FIFO enable bit preset; all 14,987,772 bytes matched the extracted vendor `Image` | Reached `0x20008000` with or without the FIFO bit; all 22,594,132 bytes matched the build's uncompressed `Image` |

With zero-filled memory standing in for UART, the vendor image was still polling the FIFO reset bits at eight seconds. The guest PC and instruction words showed a loop reading `0x13820008` and testing mask `0x6`. A second control initialized just that mock word to `1` (FIFO already enabled) for both kernels. Both reached the expanded entry with their compressed payloads unchanged. The 6.12 image SHA-256 there was `6cd80b8d6a61aa6319304dc40965972640f0f05f34cf03f70beae820c907bcb6`.

With this mapping, the measured entry SCTLR value permits 6.12 to reach its expanded instructions. The next hardware measurement that would distinguish the simulated handoff from the tablet's is the incoming translation configuration (TTBR0, TTBCR and domain access), plus actual translations of the executing image, appended DTB and prospective page-table area before replacing the bootloader table. The working vendor control provides a way to carry those measurements into recovery independently of the 6.12 USB stack.
