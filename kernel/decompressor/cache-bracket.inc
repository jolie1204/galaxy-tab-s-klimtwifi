/* SPDX-License-Identifier: GPL-2.0-only */
/* Only the first, stackless cache_on call comes here. restart reinitializes
 * the stack and r9. Keep the real decompression destination in r4. */
.Lklimt_first_cache:
		mov	r9, r4
		bl	.Lklimt_entry_fn
.Lklimt_pre_return:
		mov	r4, r9
		bl	cache_on
.Lklimt_cache_return:
		/* Validate the short-descriptor table installed by __setup_mmu
		 * before using its two Strongly-ordered retained-RAM sections. */
		mrc	p15, 0, r0, c1, c0, 0
		tst	r0, #(1 << 28)		@ TEX remap would change attributes
		bne	restart
		and	r0, r0, #5
		cmp	r0, #5
		bne	restart
		mrc	p15, 0, r0, c2, c0, 2
		cmp	r0, #0
		bne	restart
		mrc	p15, 0, r0, c2, c0, 0
		movw	r1, #0x4000
		movt	r1, #0x2000
		cmp	r0, r1
		bne	restart
		mrc	p15, 0, r2, c3, c0, 0
		and	r2, r2, #3
		cmp	r2, #1			@ domain zero is a client
		bne	restart
		add	r0, r0, #4096		@ first-level entry for 0x40000000
		ldmia	r0, {r2, r3}
		movw	r1, #0x0c12
		movt	r1, #0x4000
		cmp	r2, r1
		bne	restart
		add	r1, r1, #0x100000
		cmp	r3, r1
		bne	restart
		mov	r0, #0x40000000
		ldr	r1, [r0]
		movw	r2, #0x4244
		movt	r2, #0x4347
		cmp	r1, r2
		bne	restart
		write_after_cache
		b	restart
