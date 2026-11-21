#!/usr/bin/env python3
"""Check the corrected SM-T700 BOOT against its Linux ARM build outputs."""
import argparse
import hashlib
import lzma
import struct
from pathlib import Path


def check_zimage_bounds(zimage):
    if len(zimage) < 0x30 or struct.unpack_from('<III', zimage, 0x24) != (0x016f2818, 0, len(zimage)):
        raise ValueError('inconsistent ARM zImage bounds')


def check_vendor_payload(outer, start, end, payload):
    check_zimage_bounds(outer)
    if not 0x30 <= start < end - 4 <= len(outer) - 4:
        raise ValueError('invalid vendor input_data/input_data_end offsets')
    if struct.unpack_from('<I', outer, end - 4)[0] != len(payload):
        raise ValueError('vendor decompressor output-size trailer differs from the inner payload')
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE, memlimit=128 << 20)
    try:
        result = decoder.decompress(outer[start:end - 4], max_length=len(payload) + 1)
    except lzma.LZMAError as error:
        raise ValueError('invalid vendor LZMA stream') from error
    if not decoder.eof or decoder.unused_data or result != payload:
        raise ValueError('vendor LZMA stream differs from the expected inner zImage and DTB')


def check(boot, zimage, dtb, expanded_size, ordered_switch=False, vendor_wrapper=None):
    if len(boot) > 8 << 20 or len(boot) < 2048 or boot[:8] != b'ANDROID!':
        raise ValueError('expected an Android boot image that fits the 8 MiB BOOT partition')
    ks, ka, rs, ra, second, _, tags, page, version = struct.unpack_from('<9I', boot, 8)
    if (ka, tags, page, version, second) != (0x10008000, 0x10000100, 2048, 0, 0):
        raise ValueError('unexpected SM-T700 kernel address, tags, page size or header format')
    payload = zimage + dtb
    if vendor_wrapper is not None:
        outer, start, end = vendor_wrapper
        check_vendor_payload(outer, start, end, payload)
        payload = outer
    if ks != len(payload) or boot[page:page + ks] != payload:
        raise ValueError('BOOT kernel payload differs from the supplied build')
    check_zimage_bounds(zimage)
    if len(dtb) < 40 or struct.unpack_from('>II', dtb) != (0xd00dfeed, len(dtb)):
        raise ValueError('inconsistent appended DTB bounds')
    if b'samsung,klimt-wifi\x00' not in dtb:
        raise ValueError('the appended DTB lacks the SM-T700 compatible string')
    # The two SM-T700 wrappers have distinct ARM-state table-switch sequences.
    legacy = struct.pack('<III', 0x1e023f10, 0x1e080f17, 0x1e031f10)
    ordered = struct.pack('<7I', 0xee023f10, 0xee031f10, 0xee026f50, 0xf57ff06f,
                          0xee080f17, 0xf57ff04f, 0xf57ff06f)
    accepted = (ordered,) if ordered_switch else (legacy, ordered)
    wrapper = zimage[:4096]
    if not any(wrapper.startswith(seq, i) for i in range(0, len(wrapper), 4) for seq in accepted):
        raise ValueError('expected SM-T700 ARMv7 table-switch sequence missing')
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
    parser.add_argument('--android', action='store_true',
                        help='require the SM-T700 Android firmware node for first-stage init')
    parser.add_argument('--ordered-switch', action='store_true',
                        help='require TTBR0/DACR/TTBCR synchronization and completed TLBI')
    parser.add_argument('--vendor-wrapper', nargs=3, metavar=('ZIMAGE', 'START', 'END'),
                        help='verify the vendor zImage and its linked input_data offsets')
    for name in ('boot', 'zImage', 'dtb', 'Image'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    data = args.boot.read_bytes()
    try:
        vendor = None
        if args.vendor_wrapper:
            outer, start, end = args.vendor_wrapper
            vendor = (Path(outer).read_bytes(), int(start, 0), int(end, 0))
        size = check(data, args.zImage.read_bytes(), args.dtb.read_bytes(), args.Image.stat().st_size,
                     ordered_switch=args.ordered_switch, vendor_wrapper=vendor)
        if args.android:
            from android_firmware import require_android_dtb
            require_android_dtb(args.dtb)
    except ValueError as error:
        parser.exit(1, f'{args.boot.name}: {error}\n')
    print(f'{args.boot.name} {len(data)} bytes, external ramdisk {size} bytes, '
          f'SHA-256 {hashlib.sha256(data).hexdigest()}')
