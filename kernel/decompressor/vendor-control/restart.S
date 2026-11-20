/* SPDX-License-Identifier: MIT */
#include <asm/unistd.h>
#include <linux/reboot.h>
.syntax unified
.arm
.text
.global _start
_start:
    ldr r0, =LINUX_REBOOT_MAGIC1
    ldr r1, =LINUX_REBOOT_MAGIC2
    ldr r2, =LINUX_REBOOT_CMD_RESTART2
    adr r3, recovery
    mov r7, #__NR_reboot
    svc #0
    mov r0, #1
    mov r7, #__NR_exit
    svc #0
recovery:
    .asciz "recovery"
