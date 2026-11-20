#!/usr/bin/env python3
"""Check the corrected SM-T700 BOOT against its Linux ARM build outputs."""
import argparse
import hashlib
import struct
from pathlib import Path


def check(boot, zimage, dtb, expanded_size):
    if len(boot) > 8 << 20 or len(boot) < 2048 or boot[:8] != b'ANDROID!':
        raise ValueError('expected an Android boot image that fits the 8 MiB BOOT partition')
    ks, ka, rs, ra, second, _, tags, page, version = struct.unpack_from('<9I', boot, 8)
    if (ka, tags, page, version, second) != (0x10008000, 0x10000100, 2048, 0, 0):
        raise ValueError('unexpected SM-T700 kernel address, tags, page size or header format')
    if ks != len(zimage) + len(dtb) or boot[page:page + ks] != zimage + dtb:
        raise ValueError('BOOT kernel and appended DTB differ from the supplied build')
    if len(zimage) < 0x30 or struct.unpack_from('<III', zimage, 0x24) != (0x016f2818, 0, len(zimage)):
        raise ValueError('inconsistent ARM zImage bounds')
    if len(dtb) < 40 or struct.unpack_from('>II', dtb) != (0xd00dfeed, len(dtb)):
        raise ValueError('inconsistent appended DTB bounds')
    if b'samsung,klimt-wifi\x00' not in dtb:
        raise ValueError('the appended DTB lacks the SM-T700 compatible string')
    # TTBR0 write, unified TLB invalidate, DACR write in the ARMv7 decompressor.
    transition = struct.pack('<III', 0x1e023f10, 0x1e080f17, 0x1e031f10)
    wrapper = zimage[:4096]
    if not any(wrapper[i:i + len(transition)] == transition for i in range(0, len(wrapper), 4)):
        raise ValueError('S-BOOT TLB invalidation missing from the ARMv7 table switch')
    rd_start = (page + ks + page - 1) // page * page
    trailer = (rd_start + rs + page - 1) // page * page
    if not rs or boot[trailer:] != b'SEANDROIDENFORCE':
        raise ValueError('expected a nonempty external ramdisk and aligned Samsung trailer')
    # Lineage also hashes the empty legacy DT slot. Both v0 tools hash the empty second slot.
    digest = hashlib.sha1()
    for start, size in ((page, ks), (rd_start, rs), (0, 0)):
        digest.update(boot[start:start + size])
        digest.update(struct.pack('<I', size))
    aosp_id = digest.digest()
    digest.update(bytes(4))
    if boot[576:596] not in (aosp_id, digest.digest()) or boot[596:608] != bytes(12):
        raise ValueError('the v0 image ID does not match the packaged payloads')
    if ra != 0x18000000:
        raise ValueError('expected SM-T700 BOOT ramdisk offset +128 MiB')
    # Model the vendor kernel entry delta. Physical initrd placement needs a tablet readback.
    rd_phys = ra + 0x10000000
    if expanded_size <= 0:
        raise ValueError('empty expanded kernel')
    for start, end in ((0x20008000, 0x20008000 + expanded_size), (0x240cc000, 0x240d0000)):
        if rd_phys < end and start < rd_phys + rs:
            raise ValueError('modeled external ramdisk overlaps the kernel or incoming page table')
    return rs


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('boot', 'zImage', 'dtb', 'Image'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    data = args.boot.read_bytes()
    try:
        size = check(data, args.zImage.read_bytes(), args.dtb.read_bytes(), args.Image.stat().st_size)
    except ValueError as error:
        parser.exit(1, f'{args.boot.name}: {error}\n')
    print(f'{args.boot.name} {len(data)} bytes, external ramdisk {size} bytes, '
          f'SHA-256 {hashlib.sha256(data).hexdigest()}')
