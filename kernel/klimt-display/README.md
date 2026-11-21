# SM-T700 display bring-up on Linux 6.12

SM-T700 TWRP reported the Samsung S6E3HA1 panel ID `42:11:03` and a 1600×2560 mode at 60 Hz. The existing Linux 6.12.118 DTB leaves FIMD and DSIM1 disabled.

[The DSIM resolution patch](exynos5420-dsim-resolution.patch) adds the Exynos5420 hardware variant. It retains the original register map and the 0x58 PLL timer offset, with the 11-bit horizontal and 12-bit vertical fields defined by the Klimt downstream driver. Linux 6.12's Exynos5410 variant masks 2560 lines to 512. The Exynos5422 variant uses a different register map. The patch preserves the data and field widths of all existing variants, and leaves the current board DTB unchanged until display integration is ready.

The two affected drivers compiled to ARM EABI5 objects against Linux 6.12.118. Register-encoding checks verified 1600×2560 and the 800×2560 input to DSIM1 after MIC. No panel was driven by this patch.

The downstream Klimt board enables MIC between FIMD and DSIM1. It programs FIMD and MIC for the native width and DSIM1 for the compressed stream width. The display port still needs a 5420 MIC route, the matching S6E3HA1 power and DCS sequence, and a visible KMS test. Android also needs a compatible graphics allocator, EGL implementation and composer. Panfrost detection was previously verified on the tablet.
