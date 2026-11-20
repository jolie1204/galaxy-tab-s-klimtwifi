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

With this mapping, the measured entry SCTLR value permits 6.12 to reach its expanded instructions. The eleventh trial below measured the incoming translation configuration and selected translations in the working vendor control.

## Eleventh trial measuring the incoming address map

On November 20, 2026, the SM-T700 booted the instrumented vendor kernel from TWRP at 08:04:54 PST. Its RAM init reported Linux `3.4.113-Deathly-1.0+` and the full run-specific nineteen-word measurement at 3.81 seconds uptime. The vendor kernel logged a recovery restart at 62.61 seconds uptime. A fresh TWRP boot appeared on the original USB port at 08:06:26 PST. Three reads of the 3,738-byte previous console agreed. TWRP restored the complete pre-trial tenth BOOT and verified its SHA-256 `efcbb6a09a7e95de04185ca9b7a7241e15a512876a5d941cf5ccbdd4b3f43f73` and the original RECOVERY digest. The tablet remained in TWRP.

Before the vendor decompressor installed its own page table, the CPU reported CPSR `0x200001d3` after interrupt masking, SCTLR `0x00c5187f`, TTBCR `0`, TTBR0 `0x240cc000`, TTBR1 `0` and DACR `0x0000ffff`. All ten privileged stage-one translation requests succeeded. Each output address equaled the input at the sampled byte.

| Address queried in the vendor boot | Access queried | Raw PAR |
| --- | --- | --- |
| Executing zImage start `0x20008000` | Read | `0x20008290` |
| Boot parameter pointer in r2 `0x20000100` | Read | `0x20000290` |
| Tenth image's prospective DTB first and last bytes `0x205a2250`, `0x205af778` | Read at both | `0x205a2290`, `0x205af290` |
| Prospective page table first and last bytes `0x20004000`, `0x20007fff` | Read and write at both | `0x20004290`, `0x20007290` for both access types |
| Retained console base `0x40000000` | Read and write | `0x40000268` for both access types |

The two DTB addresses are derived from the verified tenth image size and DTB offset. The translation instructions queried permissions and output addresses without accessing the contents at these test addresses. The ARMv7 short-descriptor PAR attributes report Strongly-ordered inner memory for the image, boot parameter, prospective DTB and page-table samples, and inner/outer Write-Through for the retained console. The bootloader's complete descriptors, the contents of the tenth DTB in a 6.12 boot, and the physical execution of 6.12 remain unmeasured. [Arm ARM, section B4.1.112](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e)

For reproduction, start from the ninth trial's vendor source, toolchain and exact OTA compressed stream. Apply only the [entry hook patch](vendor-control/entry-map-head.patch) with `patch -p1` and place [entry-map.inc](vendor-control/entry-map.inc) beside `head.S`. Reuse its verified vendor command-line buffer, RAM init and reboot helper with a fresh run ID. The eleventh candidate measured 5,918,736 bytes with SHA-256 `071834a7c915c5b32d8017fef89545ef378f923e5bb1ee47408c5f0cd78e04a0`. Decode a returned console with `python3 vendor-control/read-entry-map.py CONSOLE RUN_ID`. The probe guards its own record, samples in short-descriptor SVC mode with the MMU enabled, and restores PAR before returning to the vendor code.

Host controls on a Cortex-A15 distinguished identity mappings, a DTB alias and translation/permission faults. Each preserved the incoming PAR and the expanded vendor image outside the reported command-line field. A separate emulated Linux boot exercised the packaged RAM init's complete report and recovery request. After the hardware readback, a replay used the measured SCTLR, TTBR0, TTBCR and domain access values. Both the original vendor and the exact tenth 6.12 payload reached their expanded entry with byte-identical reference images. They did so with uniform Normal sections and again with synthetic section attributes chosen to model the sampled regions. QEMU used synthetic descriptors, boot parameters and UART status. The physical checkpoint still needed is execution of the **6.12** decompressor on the SM-T700 with the incoming MMU and cache enabled. A vendor positive control for a retained marker in this state would make that checkpoint interpretable.

## Twelfth trial checking a cached marker across recovery reboot

The vendor control writes a fresh `KLIMT-VENDOR-ENTRY <run-id>` line before replacing the bootloader's translation table. It requires SVC, the MMU and data cache enabled, short descriptors and a valid vendor console signature. Before reading the signature or writing any data, it checks privileged read and write translations for each of the three pages it will touch. Each must resolve to the expected physical frame with Normal memory attributes. The writer cleans the touched words' cache lines to the point of coherency, finishes the data and parity before committing the header, and restores the incoming PAR. The complete write set is 172 bytes within the reserved 2 MiB.

To make the TWRP readback independent of vendor userspace, the successful path changes one descriptor name pointer in the **expanded vendor image in RAM**, from `ram_console` to `am_console`. The board still reserves the entire region. This prevents the running vendor driver from consuming or replacing the marker before the normal vendor reboot-to-recovery path runs. The installed recovery has its own unmodified console reader. If an entry guard declines the write, the vendor console remains active and can relay the guard status instead.

Build from the ninth trial's verified source and original OTA compressed stream. Apply the ninth trial's `guard9-head.patch`, then [cached-entry-head.patch](vendor-control/cached-entry-head.patch). Place [cached-entry.inc](vendor-control/cached-entry.inc) alongside `head.S`. Run `python3 make-marker.py --cached-vendor-run <run-id>` to produce `cached-record.inc` there, using the same fresh sixteen-digit lowercase hexadecimal ID in the RAM init. On the uncompressed vendor `Image` with SHA-256 `2b2fe646332feaedf877ffc5702902203468224b2869daa6afbcaba1709241df`, verify the descriptor at `0xc38658` contains the original pointer `0xc0ad1038` and 2 MiB size, and the board reservation starts at `0x40000000`. This run's private BOOT image measured 5,918,736 bytes with SHA-256 `13ea732b8f868e9f6eaf3dfdabdfe0d593ecdc782991adab3683f31b1fbd2560`.

The packaged decompressor passed five Cortex-A15 emulation cases. The positive case produced the exact line and all Reed–Solomon syndromes were zero. An aliased data-parity page, a write-protected header-parity page, an invalid signature and a disabled data cache each left the entire sampled area unchanged. Every case preserved PAR, passed the ARM boot arguments to the expanded vendor kernel, and matched the expected full kernel image. A separate Linux guest ran the packaged PID 1 with no previous console and issued its recovery restart. The original marker generator's default and QEMU outputs remained byte-identical.

On November 20, 2026, the host requested normal boot at 08:20:55 PST and detected a new TWRP boot at 08:22:28, 93 seconds later. Three reads of the 94-byte `/proc/last_kmsg` matched (SHA-256 `691db35672e1ddc598823c552f0a446f3387657b5d0eb87e0cf781e992677612`). The very first line was the exact vendor entry marker with this run's ID. The other text was the recovery reader's clean ECC footer and reset status `0x800`. The pre-trial logs lacked the entry line and the RAM init had no copy of the complete marker. This establishes direct retention from the cached vendor entry across this warm reboot. TWRP restored and verified the entire pre-trial tenth BOOT and original RECOVERY. The device stayed in TWRP.

### Host-prepared Linux 6.12 image using the calibrated writer

For the next physical trial, a separate image applies the [6.12 hook patch](cached-612-head.patch) to the tenth image's sixth-trial instrumented source. This patch includes the seventh trial's fixed destination in the assembly source. Copy `vendor-control/cached-entry.inc` next to `head.S` and generate `cached-record.inc` with `python3 make-marker.py --cached-612-run <new-run-id>`. Use the tenth build's kernel config, tablet DTB and timed early-ADB ramdisk. The early hook writes `KLIMT-612-ENTRY <run-id>`. After decompression and cache shutdown, the second hook can replace it with `KLIMT-612-INFLATED <run-id>`. Either exact run-specific line identifies the stage observed. Compare the ID printed by the [console reader](../early-adb/read-console.py) to the packaged ID.

The 7,045,136-byte private candidate has SHA-256 `9e15752add1ea24c62dcea233ddfba817923c13467bf92f1864ce4004a9ffcef`. In a Cortex-A15 host replay using the sampled entry register settings, two RAM snapshots captured the expected entry and inflated records with valid parity. Two further runs tested an aliased parity page at entry and an invalid signature. In all three cases the expanded kernel matched the tenth build's `Image` byte for byte and received a valid DTB pointer. The thirteenth trial below used this image on the tablet.

## Thirteenth trial with both Linux 6.12 decompressor checkpoints

On November 20, 2026 the workstation verified the candidate against its host-tested payload, the tablet DTB, the tenth build's timed ramdisk and the current ADB public key. The previous console still contained the twelfth trial's vendor marker. Both the previous console and the running recovery log lacked the thirteenth run ID. TWRP backed up the complete tenth BOOT, wrote the new image to BOOT and read back all 8 MiB after flushing. The expected and observed whole-partition SHA-256 was `3bf618a4a66f035abdcd4b0692fa2b2ed3df63df1b898bbf4117b0f060385639`. RECOVERY retained its original SHA-256 `8f673d7b01b2b481a373298ae6f73f954f611f78ff8632ff954ece1b5eca03e5`.

The host verified a separate seed in the running TWRP console and requested one normal boot at 08:50:02 PST. TWRP detached at 08:50:05. The monitoring criteria were a diagnostic ADB shell on the original USB port returning `6.12.118-klimt-adbreturn1` and UID `2000`, a new TWRP boot ID upon return, and a previous-console record with the exact candidate run ID.

The 210-second monitoring window ended at 08:53:32 PST. Both physical-port polling and the host kernel USB journal recorded just the initial disconnect. Diagnostic ADB and automatic TWRP return were unobserved in this interval. The host then requested manual TWRP entry for the retained-console readback.

At 11:55:42 PST a fresh TWRP boot appeared on the original USB port. Two reads returned the same 91-byte previous console (SHA-256 `5237cb42a577ba65a99f9089efa39059a97ab518e42848dd6f49c64b8af171ba`). It contained exactly one entry marker with this trial's run ID and zero matching inflated markers. The original TWRP seed was absent. Both partition hashes still matched the values above. This establishes execution of the instrumented Linux 6.12 decompressor entry on physical SM-T700 hardware. Delivery of a later marker, transfer to the expanded kernel, userspace and diagnostic ADB remain unverified. The USB records do not identify how the later recovery boot was triggered.

## Fourteenth trial bracketing the first cache setup

The thirteenth trial recovered the entry record. This Linux 6.12.118 diagnostic adds records immediately before and after the first conditional call to `cache_on`. The original address-selection and skip conditions still apply. The hook preserves the decompression address, boot arguments and stack pointer. The earlier entry and later post-decompression probes still run at their original stages. Each successful write replaces the preceding record in the same RAM console.

| Last retained stage | Positive observation on this boot |
| --- | --- |
| ENTRY | The processor reached the decompressor under the incoming bootloader mapping. |
| PRECACHE | The decompressor selected the first cache setup path and reached the point just before its call. |
| POSTCACHE | The first call returned and its expected page table and retained RAM mapping passed the probe checks. |
| INFLATED | Decompression returned and the post-cache-shutdown probe ran. |

A missing later stage can reflect a skipped write, failure to retain it, or an earlier stall. The precache probe uses the validated cached entry writer. The postcache probe verifies SCTLR, TTBCR, TTBR0, domain access, both expected Strongly-ordered 1 MiB descriptors and the console signature before writing. ARM specifies the incoming MMU and D-cache state, while the earlier vendor control observed both enabled on this tablet. [ARM Linux boot protocol](https://docs.kernel.org/arch/arm/booting.html#calling-the-kernel-image)

Apply [the first-call patch](cache-bracket.patch) after the thirteenth trial's hook patch and place [cache-bracket.inc](cache-bracket.inc) beside head.S. Generate cached-record.inc with `python3 make-marker.py --cache-bracket-run <sixteen-lowercase-hex-digits>`. Reuse the thirteenth trial's tablet DTB, expanded kernel and authenticated timed early-ADB ramdisk. The first call site and restart offsets stayed at 0xf4 and 0xf8. The compressed wrapper grew by 864 bytes. The private BOOT image measured 7,045,136 bytes, SHA-256 `62f9b5581573d55ce4d4f79e11a62b444a0514dff3dcf095676372e3fa293e12`.

Four Cortex-A15 replays sampled the retained region at all four stages. They covered the normal path, an incoming aliased parity page, an invalid console signature and a changed section descriptor after the table switch. The last case retained PRECACHE at the postcache sample. The invalid signature left the full sampled region intact. Each run checked parity and write bounds for the applicable records, delivered the tablet DTB and reached the same expanded kernel bytes (SHA-256 `6cd80b8d6a61aa6319304dc40965972640f0f05f34cf03f70beae820c907bcb6`).

TWRP 3.7.0_9-0 backed up the complete preceding BOOT. After the new image was written and flushed, all 8 MiB read back with SHA-256 `42e4983d917cb3bf7a822df46cf12f95c52f3d6f97336273f6a10d0a179dcd50`. RECOVERY matched its original digest. A single normal boot was requested at 13:49:30 PST on November 20, 2026. The original physical USB port detached in the same second.

The 210-second window ended at 13:53:00 PST. Both the physical-port monitor and the USB kernel journal saw only the initial disconnect. No diagnostic ADB shell or new TWRP boot appeared during this interval. Manual TWRP entry was requested for the retained-console readback.
