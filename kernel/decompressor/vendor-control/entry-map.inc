/* SPDX-License-Identifier: GPL-2.0-only */
/* Vendor ARM-state decompressor, before it builds its own translation table.
 * r7/r8/r9, sp and lr stay intact. ATS requests do not access the target data.
 * The nineteen output words are described in the trial record. */
.macro map_par op
    mcr p15, 0, r1, c7, c8, \op
    isb
    mrc p15, 0, r2, c7, c4, 0
    str r2, [r3], #4
.endm

.Lmap_sample:
    mrs r6, cpsr
    and r0, r6, #31
    cmp r0, #0x13                      @ SVC short-descriptor measurements only
    movne r5, #2
    bxne lr
    mrc p15, 0, r0, c2, c0, 2
    tst r0, #0x80000000
    movne r5, #3
    bxne lr
    mrc p15, 0, r0, c1, c0, 0
    tst r0, #1
    moveq r5, #6
    bxeq lr

    adrl r3, .Lmap_record
    adr r0, .Lmap_sample
    add r1, r3, #75
    eor r1, r1, r0
    eor r2, r3, r0
    orr r1, r1, r2
    movs r1, r1, lsr #12               @ write only within the executing code page
    movne r5, #5
    bxne lr
    mrrc p15, 0, r10, r11, c7          @ preserve PAR, including its upper word
    mcr p15, 0, r3, c7, c8, 1          @ privileged write permission on our record
    isb
    mrc p15, 0, r0, c7, c4, 0
    tst r0, #1
    movne r5, #4
    bne .Lmap_restore_par

    add r3, r3, #4                     @ status travels in r5 across cache_on
    str r6, [r3], #4
    mrc p15, 0, r0, c1, c0, 0
    str r0, [r3], #4
    mrc p15, 0, r0, c2, c0, 2
    str r0, [r3], #4
    mrc p15, 0, r0, c2, c0, 0
    str r0, [r3], #4
    mrc p15, 0, r0, c2, c0, 1
    str r0, [r3], #4
    mrc p15, 0, r0, c3, c0, 0
    str r0, [r3], #4
    adrl r0, .Lmap_record
    ldr r1, .Lmap_link_offset
    sub r5, r0, r1
    str r5, [r3], #4
    str r8, [r3], #4
    mov r1, r5
    map_par 0
    mov r1, r8
    map_par 0
    ldr r0, .Lmap_dtb_first
    add r1, r5, r0
    map_par 0
    ldr r0, .Lmap_dtb_last
    add r1, r5, r0
    map_par 0
    ldr r1, .Lmap_table_first
    map_par 0
    map_par 1
    add r1, r1, #16384
    sub r1, r1, #1
    map_par 0
    map_par 1
    mov r1, #0x40000000
    map_par 0
    map_par 1

    adrl r1, .Lmap_record
1:  mcr p15, 0, r1, c7, c10, 1         @ clean our record before changing mappings
    add r1, r1, #4
    cmp r1, r3
    blo 1b
    dsb sy
    mov r5, #1
.Lmap_restore_par:
    mcrr p15, 0, r10, r11, c7
    isb
    bx lr

.Lmap_commit:
    adr r0, .Lmap_record
    str r5, [r0]
    bx lr
.Lmap_link_offset: .word .Lmap_record
.Lmap_dtb_first:   .word 0x0059a250    @ offsets from the tenth packaged zImage base
.Lmap_dtb_last:    .word 0x005a7778
.Lmap_table_first: .word 0x20004000
.Lmap_record:      .fill 19, 4, 0

/* After relocation, decompression and cache shutdown. The vendor command-line
 * buffer offset, available length and original contents are verified on host. */
.Lmap_report:
    adr r3, .Lmap_record
    ldr r2, .Lmap_cmdline_tail
    add r2, r4, r2
    ldrb r0, [r2]
    cmp r0, #0
    bxne lr
    adr r1, .Lmap_tag
1:  ldrb r0, [r1], #1
    cmp r0, #0
    strneb r0, [r2], #1
    bne 1b
    mov r9, #19
2:  ldr r0, [r3], #4
    mov ip, #8
3:  mov r0, r0, ror #28
    and r1, r0, #15
    cmp r1, #10
    addlo r1, r1, #48
    addhs r1, r1, #87
    strb r1, [r2], #1
    subs ip, ip, #1
    bne 3b
    subs r9, r9, #1
    movne r1, #44
    strneb r1, [r2], #1
    bne 2b
    mov r1, #0
    strb r1, [r2]
    dsb sy
    bx lr
.Lmap_cmdline_tail: .word 0x00c376cd
.Lmap_tag:          .asciz " klimtdiag="
.balign 4
