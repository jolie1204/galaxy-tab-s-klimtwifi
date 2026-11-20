#!/usr/bin/env python3
"""Package a klimtwifi kernel built with the platform BOOT's exact Android ramdisk."""
import argparse
import lzma
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def assemble(boot, kernel_out, tools, output):
    original = boot.read_bytes()
    if output.resolve() == boot.resolve():
        raise ValueError('use a separate output for the embedded variant')
    if len(original) < 2048 or original[:8] != b'ANDROID!':
        raise ValueError('expected an Android BOOT header')
    if struct.unpack_from('<I', original, 20)[0] != 0x18000000:
        raise ValueError('start from the corrected +128 MiB platform BOOT')
    trailer = b'SEANDROIDENFORCE'
    with tempfile.TemporaryDirectory(dir=output.parent) as tmp:
        work = Path(tmp).resolve()
        args = subprocess.check_output([
            sys.executable, tools / 'unpack_bootimg.py', '--boot_img', boot,
            '--out', work, '--format=mkbootimg', '-0',
        ]).split(b'\0')
        pack = [sys.executable, tools / 'mkbootimg.py', *map(os.fsdecode, args[:-1]),
                '--output', work / 'packed.img']
        subprocess.run(pack, check=True)
        if (work / 'packed.img').read_bytes() + trailer != original:
            raise ValueError('mkbootimg did not reproduce the original platform BOOT')
        arm = kernel_out / 'arch/arm/boot'
        zimage = (arm / 'zImage').read_bytes()
        expanded = (arm / 'Image').read_bytes()
        ramdisk = work / 'ramdisk'
        cpio = lzma.decompress(ramdisk.read_bytes())
        if not cpio.startswith(b'070701') or expanded.count(cpio) != 1:
            raise ValueError('the expanded kernel must embed this exact Android newc archive once')
        kernel = work / 'kernel'
        previous = kernel.read_bytes()
        if len(previous) < 0x30:
            raise ValueError('invalid kernel payload in the platform BOOT')
        dtb = previous[struct.unpack_from('<I', previous, 0x2c)[0]:]
        if len(dtb) < 40 or struct.unpack_from('>II', dtb) != (0xd00dfeed, len(dtb)):
            raise ValueError('could not locate the original tablet DTB')
        kernel.write_bytes(zimage + dtb)
        empty = subprocess.check_output(['cpio', '--quiet', '-o', '-H', 'newc'], input=b'')
        ramdisk.write_bytes(lzma.compress(empty, check=lzma.CHECK_CRC32, preset=0))
        subprocess.run(pack, check=True)
        candidate = work / 'packed.img'
        with candidate.open('ab') as image:
            image.write(trailer)
        device_tree = work / 'tablet.dtb'
        device_tree.write_bytes(dtb)
        checker = Path(__file__).resolve().parents[1] / 'kernel/check-linux612-boot.py'
        subprocess.run([sys.executable, checker, candidate, arm / 'zImage',
                        device_tree, arm / 'Image'], check=True)
        candidate.replace(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('boot', 'kernel_out', 'mkbootimg_dir', 'output'):
        parser.add_argument(arg, type=Path)
    args = parser.parse_args()
    try:
        assemble(args.boot, args.kernel_out, args.mkbootimg_dir, args.output)
    except ValueError as error:
        parser.exit(1, f'{error}\n')
