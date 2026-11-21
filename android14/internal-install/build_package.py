#!/usr/bin/env python3
"""Build a signed, full-partition TWRP ZIP from verified SM-T700 images."""

import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

BOOT_SIZE = 8_388_608
SYSTEM_SIZE = 2_527_068_160
UPDATER = "META-INF/com/google/android/update-binary"


def digest(source):
    return hashlib.file_digest(source, "sha256").hexdigest()


def audit_inputs(boot, system):
    if boot.stat().st_size != BOOT_SIZE or system.stat().st_size != SYSTEM_SIZE:
        raise ValueError("BOOT or SYSTEM size differs from the SM-T700 partition layout")
    with boot.open("rb") as file:
        header = file.read(48)
        if header[:8] != b"ANDROID!":
            raise ValueError("BOOT lacks the Android boot header")
        kernel, _, ramdisk, _, _, _, _, page, version = struct.unpack_from("<9I", header, 8)
        if page != 2048 or version != 0 or not kernel or not ramdisk:
            raise ValueError("BOOT does not use the audited Android v0 layout")
        end = page + sum((size + page - 1) // page * page for size in (kernel, ramdisk))
        if end + 16 > BOOT_SIZE:
            raise ValueError("BOOT payload exceeds the partition")
        file.seek(end)
        if file.read(16) != b"SEANDROIDENFORCE":
            raise ValueError("BOOT lacks the Samsung trailer")
    with system.open("rb") as file:
        file.seek(1080)
        if file.read(2) != bytes.fromhex("53ef"):
            raise ValueError("SYSTEM is not a raw ext4 image")
    subprocess.run(["e2fsck", "-fn", system], check=True)


def write_package(args):
    if args.output.exists():
        raise FileExistsError(args.output)
    audit_inputs(args.boot, args.system)
    hashes = {}
    for name, path in (("boot", args.boot), ("system", args.system)):
        with path.open("rb") as file:
            hashes[name] = digest(file)

    script = (Path(__file__).parent / "update-binary.sh.in").read_text()
    replacements = {
        "@BOOT_SIZE@": str(BOOT_SIZE),
        "@SYSTEM_SIZE@": str(SYSTEM_SIZE),
        "@BOOT_SHA@": hashes["boot"],
        "@SYSTEM_SHA@": hashes["system"],
    }
    for key, value in replacements.items():
        script = script.replace(key, value)
    if re.search(r"@[A-Z_]+@", script):
        raise ValueError("Unresolved updater template field")
    subprocess.run(["sh", "-n"], input=script, text=True, check=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent, prefix="klimt-package-") as folder:
        unsigned = Path(folder) / "unsigned.zip"
        signed = Path(folder) / "signed.zip"
        with zipfile.ZipFile(unsigned, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=3) as package:
            package.writestr(UPDATER, script)
            package.write(args.boot, "boot.img")
            package.write(args.system, "system.img")
        subprocess.run([args.signapk, "-w", args.cert, args.key, unsigned, signed], check=True)
        with zipfile.ZipFile(signed) as package:
            if package.read(UPDATER) != script.encode():
                raise ValueError("Updater changed during signing")
            for name, size in (("boot", BOOT_SIZE), ("system", SYSTEM_SIZE)):
                entry = name + ".img"
                if package.getinfo(entry).file_size != size:
                    raise ValueError(f"Unexpected packaged size for {entry}")
                with package.open(entry) as member:
                    if digest(member) != hashes[name]:
                        raise ValueError(f"Payload readback mismatch for {entry}")
        signed.rename(args.output)

    with args.output.open("rb") as file:
        print(f"{digest(file)}  {args.output.name}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("boot", "system", "output", "signapk", "cert", "key"):
        parser.add_argument("--" + name, type=Path, required=True)
    write_package(parser.parse_args())
