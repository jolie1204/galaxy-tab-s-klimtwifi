/* SPDX-License-Identifier: GPL-2.0-only */
/* Entry in ARM state. Preserve r7/r8/r9, sp and lr. The caller carries
 * r5 (SCTLR) and r6 (verdict) through the vendor cache setup. */
.macro check_ring_access op
    mcr p15, 0, r2, c7, c8, \op
    isb
    mrc p15, 0, r1, c7, c4, 0
    tst r1, #3                         @ fault or supersection
    bne .Lcached_restore
    tst r1, #0x800                     @ only short PAR format
    bne .Lcached_restore
    eor r4, r1, r2
    movs r4, r4, lsr #12               @ exact physical frame
    bne .Lcached_restore
    ands r4, r1, #0x70                 @ Normal non-cacheable or Normal cacheable
    beq 1f
    cmp r4, #0x50
    blo .Lcached_restore
1:
.endm
.macro check_ring_page offset, verdict
    mov r6, #\verdict
    movw r2, #((\offset) & 65535)
    movt r2, #((\offset) >> 16)
    add r2, r0, r2
    check_ring_access 0                @ privileged read
    check_ring_access 1                @ privileged write
.endm

    mrc p15, 0, r5, c1, c0, 0
    mrs r1, cpsr
    and r1, r1, #31
    cmp r1, #0x13
    movne r6, #2
    bxne lr
    and r1, r5, #5
    cmp r1, #5
    movne r6, #3
    bxne lr
    mrc p15, 0, r1, c2, c0, 2
    tst r1, #0x80000000
    movne r6, #4
    bxne lr

    mrrc p15, 0, r10, r11, c7
    mov r0, #0x40000000
    check_ring_page 0, 5
    check_ring_page KLIMT_DATA_ECC, 6
    check_ring_page KLIMT_HEADER_ECC, 7
    ldr r1, [r0]
    movw r2, #0x4244
    movt r2, #0x4347
    cmp r1, r2
    movne r6, #8
    bne .Lcached_restore
    write_cached_record
    mov r6, #1
.Lcached_restore:
    mcrr p15, 0, r10, r11, c7
    isb
    bx lr
