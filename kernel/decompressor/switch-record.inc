/* SPDX-License-Identifier: GPL-2.0-only */
/* The caller provides the stage in r6 and keeps r4/r5/r7/r8, sp and lr.
 * Limit physical writes to the already initialized vendor console. */
.macro klimt_identity op
    mcr p15, 0, r2, c7, c8, \op
    isb
    mrc p15, 0, r1, c7, c4, 0
    tst r1, #3                         @ fault or supersection
    bne .Lklimt_walk_restore
    tst r1, #0x800                     @ long descriptor PAR
    bne .Lklimt_walk_restore
    eor ip, r1, r2
    movs ip, ip, lsr #12
    bne .Lklimt_walk_restore
.endm

.macro klimt_console_page offset
    movw r2, #((\offset) & 65535)
    movt r2, #((\offset) >> 16)
    add r2, r0, r2
    .irp op, 0, 1                     @ privileged read and write
        klimt_identity \op
        and ip, r1, #0x7c             @ effective inner/outer attributes
        cmp ip, #0                    @ Normal non-cacheable
        cmpne ip, #0x10               @ Strongly-ordered
        bne .Lklimt_walk_restore
    .endr
.endm

.Lklimt_walk_record:
    mrs r1, cpsr
    and r1, r1, #31
    cmp r1, #0x13
    bxne lr
    mrc p15, 0, r1, c1, c0, 0
    tst r1, #(1 << 28)
    bxne lr
    and r1, r1, #5
    cmp r1, #5
    bxne lr
    mrc p15, 0, r1, c2, c0, 2
    cmp r1, #0
    bxne lr
    mrc p15, 0, r1, c3, c0, 0
    and r1, r1, #3
    cmp r1, #1                        @ domain zero, client access
    bxne lr
    mrc p15, 0, r2, c2, c0, 0
    mov r2, r2, lsr #14
    movw r1, #0x8001                  @ 0x20004000 >> 14
    cmp r2, r1
    bxne lr
    mrrc p15, 0, r9, r10, c7
    movw r2, #0x5000
    movt r2, #0x2000
    klimt_identity 0
    ldmia r2, {r1, r3}
    movw ip, #0x73ef                  @ physical section, type, TEX/C/B, domain
    movt ip, #0xfff4
    and r1, r1, ip
    and r3, r3, ip
    movw r2, #2
    movt r2, #0x4000
    cmp r1, r2
    bne .Lklimt_walk_restore
    add r2, r2, #0x100000
    cmp r3, r2
    bne .Lklimt_walk_restore
    mov r0, #0x40000000
    klimt_console_page 0
    klimt_console_page KLIMT_DATA_ECC
    klimt_console_page KLIMT_HEADER_ECC
    ldr r1, [r0]
    movw r2, #0x4244
    movt r2, #0x4347
    cmp r1, r2
    bne .Lklimt_walk_restore
    cmp r6, #0
    bne .Lklimt_before_inflate
    write_live_table
    b .Lklimt_walk_restore
.Lklimt_before_inflate:
    write_before_inflate
.Lklimt_walk_restore:
    mcrr p15, 0, r9, r10, c7
    isb
    bx lr
