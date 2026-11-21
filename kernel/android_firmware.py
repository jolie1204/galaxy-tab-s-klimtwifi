"""Verify the Android firmware node in the SM-T700 ARM kernel prebuilt."""
import argparse
import struct
import subprocess
import tempfile
from pathlib import Path


def require_android_dtb(dtb, fdtget='fdtget'):
    for node, key, value in (('/', 'compatible', 'samsung,klimt-wifi'),
                             ('/firmware/android', 'compatible', 'android,firmware'),
                             ('/firmware/android', 'hardware', 'universal5420')):
        try:
            prop = subprocess.run([fdtget, '-t', 's', dtb, node, key],
                                  capture_output=True)
        except FileNotFoundError as error:
            raise ValueError('install fdtget to verify Android firmware properties') from error
        matches = (value.encode() in prop.stdout.split() if node == '/'
                   else prop.stdout == (value + '\n').encode())
        if prop.returncode or not matches:
            raise ValueError(f'expected {node.rstrip("/")}/{key} = {value} in the BOOT DTB')


def check_prebuilt(kernel, fdtget):
    data = kernel.read_bytes()
    if len(data) < 0x30:
        raise ValueError('truncated ARM zImage')
    magic, start, end = struct.unpack_from('<III', data, 0x24)
    if magic != 0x016f2818 or start != 0 or not 0x30 <= end <= len(data) - 40:
        raise ValueError('expected a klimtwifi zImage with one appended DTB')
    if struct.unpack_from('>II', data, end) != (0xd00dfeed, len(data) - end):
        raise ValueError('invalid appended DTB length')
    with tempfile.TemporaryDirectory() as folder:
        dtb = Path(folder) / 'board.dtb'
        dtb.write_bytes(data[end:])
        require_android_dtb(dtb, fdtget)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kernel', type=Path)
    parser.add_argument('fdtget')
    args = parser.parse_args()
    try:
        check_prebuilt(args.kernel, args.fdtget)
    except ValueError as error:
        parser.exit(1, f'{args.kernel}: {error}\n')
