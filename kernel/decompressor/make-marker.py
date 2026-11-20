#!/usr/bin/env python3
"""Generate two stackless ARMv7 probes for an SM-T700 diagnostic build."""
import argparse
import re
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


def copy_words(offset, data, clean):
    assert len(data) % 4 == 0 and offset + len(data) <= SIZE
    label = f".Lklimt_copy_{offset}_\\@"
    print(f"""    movw r2, #{offset & 0xffff}
    movt r2, #{offset >> 16}
    add r2, r0, r2
    mov r3, #{len(data) // 4}
{label}:
    ldr ip, [r1], #4""")
    if clean:
        print("    str ip, [r2]\n    mcr p15, 0, r2, c7, c10, 1\n    add r2, r2, #4")
    else:
        print("    str ip, [r2], #4")
    print(f"""    subs r3, r3, #1
    bne {label}""")


def emit_record(message, clean=False):
    data = message.encode('ascii') + b'\n'
    assert len(data) <= BLOCK
    header = struct.pack('<III', MAGIC, len(data), len(data))
    block = data.ljust(BLOCK, b'\0')
    segments = [(12, block), (FIRST_PARITY, parity(block)),
                (SIZE - ECC, parity(header)), (0, header)]
    print("    adr r1, .Lklimt_record\\@")
    for offset, content in segments:
        if offset == 0:
            print("    dsb sy")
        copy_words(offset, content, clean)
    print("    dsb sy\n    b .Lklimt_done\\@\n    .balign 4\n.Lklimt_record\\@:")
    for _, content in segments:
        print("    .byte " + ", ".join(str(x) for x in content))
    print(".Lklimt_done\\@:\n.endm")


def macro(name, message, base):
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
    bne .Lklimt_done\\@""")
    emit_record(message)


def cache_bracket_records(run_id):
    for name, stage, clean in [('write_at_entry', 'ENTRY', True),
                               ('write_before_cache', 'PRECACHE', True),
                               ('write_after_cache', 'POSTCACHE', False)]:
        print(f".macro {name}")
        emit_record(f'KLIMT-612-{stage} {run_id}', clean)
    # Both callers use the same guarded writer. Its return address
    # identifies the call site without using a stack or a live register.
    print(""".macro write_cached_record
    adr r1, 1f
    ldr r2, [r1]
    add r1, r1, r2
    cmp lr, r1
    beq 2f
    write_at_entry
    b 3f
1:  .word .Lklimt_pre_return - 1b
2:  write_before_cache
3:
.endm""")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--qemu-virt', action='store_true',
                      help='use the reserved region in the separate ARM emulator test')
    mode.add_argument('--cached-vendor-run', metavar='HEX_ID',
                      help='emit the cached vendor record for a 16-digit run ID')
    mode.add_argument('--cached-612-run', metavar='HEX_ID',
                      help='emit distinct cached entry and post-decompression records for Linux 6.12')
    mode.add_argument('--cache-bracket-run', metavar='HEX_ID',
                      help='also mark the first call to cache_on and its return on SM-T700')
    args = parser.parse_args()
    run_id = next((value for value in (args.cached_vendor_run, args.cached_612_run,
                                       args.cache_bracket_run) if value is not None), None)
    if run_id is not None:
        if not re.fullmatch(r'[0-9a-f]{16}', run_id):
            parser.error('expected sixteen lowercase hexadecimal digits')
        print(f".equ KLIMT_DATA_ECC, {FIRST_PARITY}\n.equ KLIMT_HEADER_ECC, {SIZE - ECC}")
        if args.cache_bracket_run is not None:
            cache_bracket_records(run_id)
        else:
            print(".macro write_cached_record")
            prefix = 'KLIMT-VENDOR-ENTRY ' if args.cached_vendor_run is not None else 'KLIMT-612-ENTRY '
            emit_record(prefix + run_id, clean=True)
        if args.cached_vendor_run is None:
            macro('klimt_mark_inflated', 'KLIMT-612-INFLATED ' + run_id, 0x40000000)
    else:
        base = 0x4d000000 if args.qemu_virt else 0x40000000
        macro('klimt_mark_entry', 'KLIMT-HEAD6 A decompressor entry', base)
        macro('klimt_mark_inflated', 'KLIMT-HEAD6 B decompressed image', base)
