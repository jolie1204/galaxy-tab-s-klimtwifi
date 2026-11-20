#!/usr/bin/env python3
"""Generate two stackless ARMv7 probes for an SM-T700 diagnostic build."""
import argparse
import struct

SIZE = 2 << 20
ECC = 16
BLOCK = 128
MAGIC = 0x43474244
BLOCKS = (SIZE - 12 - ECC + BLOCK + ECC - 1) // (BLOCK + ECC)
FIRST_PARITY = SIZE - (BLOCKS + 1) * ECC


def multiply(a, b):
    result = 0
    while b:
        if b & 1:
            result ^= a
        a = (a << 1) ^ (0x11D if a & 0x80 else 0)
        b >>= 1
    return result


def parity(data):
    """Return the vendor console RS(255,239) parity over GF(256)."""
    polynomial = [1]
    root = 1
    for _ in range(ECC):
        product = [0] * (len(polynomial) + 1)
        for i, coefficient in enumerate(polynomial):
            product[i] ^= coefficient
            product[i + 1] ^= multiply(coefficient, root)
        polynomial = product
        root = multiply(root, 2)
    remainder = list(data) + [0] * ECC
    for i in range(len(data)):
        leading = remainder[i]
        for j, coefficient in enumerate(polynomial):
            remainder[i + j] ^= multiply(leading, coefficient)
    return bytes(remainder[-ECC:])


def copy_words(offset, data):
    assert len(data) % 4 == 0 and offset + len(data) <= SIZE
    label = f".Lklimt_copy_{offset}_\\@"
    print(f"""    movw r2, #{offset & 0xffff}
    movt r2, #{offset >> 16}
    add r2, r0, r2
    mov r3, #{len(data) // 4}
{label}:
    ldr ip, [r1], #4
    str ip, [r2], #4
    subs r3, r3, #1
    bne {label}""")


def macro(name, message, base):
    data = message.encode('ascii') + b'\n'
    header = struct.pack('<III', MAGIC, len(data), len(data))
    block = data.ljust(BLOCK, b'\0')
    segments = [(12, block), (FIRST_PARITY, parity(block)),
                (SIZE - ECC, parity(header)), (0, header)]
    # r7/r8 hold the ARM boot arguments. Use only scratch registers.
    print(f""".macro {name}
    mrc p15, 0, r0, c1, c0, 0
    tst r0, #5
    bne .Lklimt_done\\@
    movw r0, #{base & 0xffff}
    movt r0, #{base >> 16}
    ldr r1, [r0]
    movw r2, #{MAGIC & 0xffff}
    movt r2, #{MAGIC >> 16}
    cmp r1, r2
    bne .Lklimt_done\\@
    adr r1, .Lklimt_record\\@""")
    for offset, content in segments:
        if offset == 0:
            print("    dsb sy")
        copy_words(offset, content)
    print("    dsb sy\n    b .Lklimt_done\\@\n    .balign 4\n.Lklimt_record\\@:")
    for _, content in segments:
        print("    .byte " + ", ".join(str(x) for x in content))
    print(".Lklimt_done\\@:\n.endm")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu-virt', action='store_true',
                        help='use the reserved region in the separate ARM emulator test')
    args = parser.parse_args()
    base = 0x4d000000 if args.qemu_virt else 0x40000000
    macro('klimt_mark_entry', 'KLIMT-HEAD6 A decompressor entry', base)
    macro('klimt_mark_inflated', 'KLIMT-HEAD6 B decompressed image', base)
