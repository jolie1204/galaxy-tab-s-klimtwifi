#!/usr/bin/env python3
"""Build the SM-T700 vendor entry control from the verified October BOOT."""
import hashlib
import struct
import sys
from pathlib import Path

PAGE = 2048
SOURCE = '5dbe57bb2975236c084af24ff1344e32cc8fb5e8d82faf046a2318fcfed5081d'


def align(size):
    return (size + PAGE - 1) // PAGE * PAGE


def arm_branch(origin, target):
    displacement = target - origin - 8
    assert displacement % 4 == 0 and -(1 << 25) <= displacement < (1 << 25)
    return struct.pack('<I', 0xea000000 | ((displacement >> 2) & 0xffffff))


def package(template, kernel, ramdisk):
    header = bytearray(template[:PAGE])
    struct.pack_into('<I', header, 8, len(kernel))
    struct.pack_into('<I', header, 16, len(ramdisk))
    digest = hashlib.sha1()
    for item in (kernel, ramdisk, b'', b''):
        digest.update(item)
        digest.update(struct.pack('<I', len(item)))
    header[576:596] = digest.digest()
    return header + kernel.ljust(align(len(kernel)), b'\0') + ramdisk.ljust(align(len(ramdisk)), b'\0') + b'SEANDROIDENFORCE'


def main():
    boot_file, stub_file, ramdisk_file, output = map(Path, sys.argv[1:])
    source = boot_file.read_bytes()
    assert hashlib.sha256(source).hexdigest() == SOURCE
    ksize, rsize = struct.unpack_from('<I4xI', source, 8)
    rstart = PAGE + align(ksize)
    original = source[PAGE:PAGE + ksize]
    assert package(source, original, source[rstart:rstart + rsize]) == source
    # The four bytes just before the vendor's PC-relative load are alignment padding.
    assert struct.unpack_from('<4I', original, 0x54) == (0xe121f002, 0, 0, 0xe59f4784)
    assert struct.unpack_from('<I', original, 0x2c)[0] == ksize
    stub = stub_file.read_bytes()
    result = bytearray(original)
    result[0x5c:0x60] = arm_branch(0x5c, ksize)
    result += stub + arm_branch(ksize + len(stub), 0x60)
    struct.pack_into('<I', result, 0x2c, len(result))
    assert len(result) < 16 * 1024 * 1024
    candidate = package(source, result, ramdisk_file.read_bytes())
    assert len(candidate) < 8 * 1024 * 1024
    output.write_bytes(candidate)


if __name__ == '__main__':
    main()
