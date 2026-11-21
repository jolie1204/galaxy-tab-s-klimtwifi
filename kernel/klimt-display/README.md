# SM-T700 display bring-up on Linux 6.12

SM-T700 TWRP reported the Samsung S6E3HA1 panel ID `42:11:03` and a 1600×2560 mode at 60 Hz. The existing Linux 6.12.118 DTB leaves FIMD and DSIM1 disabled.

[The DSIM resolution patch](exynos5420-dsim-resolution.patch) adds the Exynos5420 hardware variant. It retains the original register map and the 0x58 PLL timer offset, with the 11-bit horizontal and 12-bit vertical fields defined by the Klimt downstream driver. Linux 6.12's Exynos5410 variant masks 2560 lines to 512. The Exynos5422 variant uses a different register map. The patch preserves the data and field widths of all existing variants, and leaves the current board DTB unchanged until display integration is ready.

The two affected drivers compiled to ARM EABI5 objects against Linux 6.12.118. Register-encoding checks verified 1600×2560 and the 800×2560 input to DSIM1 after MIC. No panel was driven by this patch.

The downstream Klimt board enables MIC between FIMD and DSIM1. It programs FIMD and MIC for the native width and DSIM1 for the compressed stream width. The display port still needs a 5420 MIC route, the matching S6E3HA1 power and DCS sequence, and a visible KMS test. Android also needs a compatible graphics allocator, EGL implementation and composer. Panfrost detection was previously verified on the tablet.

## Physical framebuffer and DCS test, trial 32

Apply [the MIC and panel probe patch](klimt-mic-panel-probe.patch) after the resolution patch. Set `CONFIG_DRM_EXYNOS_MIC=y` and `CONFIG_DRM_PANEL_SAMSUNG_S6E3HA1_PROBE=y` in the existing SM-T700 diagnostic config. Compile the target's DTB and retain the verified Android firmware, ramoops, software recovery and USB 2 LPM settings. The optional panel driver implements the ordered rails and reset and attempts DCS ID 0x04. Display-on and brightness programming remain disabled.

Linux 6.12.118 registered Exynos DRM minor 0 and the `exynosdrmfb` framebuffer on the SM-T700. Panfrost registered independently. The Exynos5420 DSI host attached a four-lane RGB888 panel device, but DCS ID 0x04 timed out at uptime 4.222 s. Three reads of the retained console matched the live USB ACM result. Actual panel response and pixels remain unverified. TWRP returned by software. Full BOOT and original RECOVERY readbacks matched; full SYSTEM and EFS comparisons changed only six primary ext4 mount metadata bytes each, and both filesystems were clean.

Two source patches replayed against 6.12.118 with zero fuzz. The ARM linked image and device tree were verified, and all three packed-image replays matched. The next DCS test uses the downstream host initialization and high-speed command order.

The 7553040-byte probe BOOT had SHA-256 `33db951b73763bb65f3b4b4eaeb44356cef291f7d65b45544ab8b20755dd1180`; the full partition readback was `be7b1f24cefb21d135078d58679e64009b107e6d9269f5b4692f03da5b6abdaf`.

## Link order and 512 MHz D-PHY, trials 33 and 34

[The follow-up diagnostic patch](klimt-dphy-link-test.patch) reproduces the downstream high-speed DCS transfer order, waits for reset and settling, and takes the existing Exynos register table values for the measured 512 MHz link. It programs the analog PHY before the PLL is enabled. The raw board DTB was unchanged from trial 32.

Trial 33 initialized the host at 512 MHz with escape divider 5. High-speed DCS ID 0x04 still timed out. Trial 34 read back the downstream D-PHY values `0xaf`, `0x060b`, `0x07270d08`, and `0x090d0b`. The same command timed out again. Both runs registered the Exynos framebuffer and returned to TWRP in software. Their full BOOT and original RECOVERY readbacks matched. Whole SYSTEM/EFS comparisons found only the six known primary ext4 mount metadata byte changes on each, and both filesystems were clean. Neither run produced an Android frame or verified physical panel output.

## Physical power and DSI receive diagnostics, trial 35

Apply [the receive diagnostics](klimt-dsi-rx-diagnostics.patch) after the link test patch. The live S6E3HA1 probe confirmed all three panel supplies enabled at 1.8, 3.0 and 1.6 V and reset deasserted. Both automatic and explicit bus turnaround timed out on DCS ID 0x04 with no receive interrupt. Data lane 0 had left stop state at the timeout in both cases. These observations locate the remaining failure on the DSI response path. They do not establish a root cause.

Exynos DRM registration and software return to TWRP were again observed. Full BOOT and RECOVERY hashes matched, the SYSTEM and EFS payloads were unchanged past block zero, and their six changed ext4 metadata bytes match earlier recovery boots. Stock TWRP also exposes the separate bootloader framebuffer range `0x67000000–0x67f9ffff`. Which framebuffer the firmware left scanning is the next readback.
