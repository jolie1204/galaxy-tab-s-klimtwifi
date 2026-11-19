#!/usr/bin/env python3
"""Repack the verified SM-T700 LineageOS 21 full block OTA."""

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile


BASE_SHA256 = "4f124ba6a49b87770ddfd23615ebc2ea1b36048f391453d215aaac972e78c661"
BLOCK = 4096
SYSTEM_BYTES = 2527068160
SYSTEM_FS_BLOCKS = 611840
BOOT_BYTES = 8388608
METADATA = "META-INF/com/android/metadata"
UPDATER = "META-INF/com/google/android/updater-script"
NEW_DATA = "system.new.dat.br"
TRANSFER = "system.transfer.list"
CERTIFICATE = "META-INF/com/android/otacert"
APPS = (
    "/system/product/app/ExactCalculator",
    "/system/product/priv-app/Eleven",
    "/system/product/app/Recorder",
    "/system/product/app/Etar",
)
RECOVERY_FILES = (
    "/system/vendor/bin/install-recovery.sh",
    "/system/vendor/etc/recovery.img",
)


def run(*command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def debug(image, command):
    return run("debugfs", "-R", command, image, stdout=subprocess.PIPE, text=True,
               stderr=subprocess.DEVNULL).stdout


def tree(image, directory):
    children = []
    for line in debug(image, f"ls -p {directory}").splitlines():
        if not line.startswith("/"):
            continue
        _, _, mode, _, _, name, *_ = line.split("/")
        if name in (".", ".."):
            continue
        if not re.fullmatch(r"[A-Za-z0-9_.+-]+", name):
            raise ValueError(f"Unexpected path component in {directory}: {name}")
        path = f"{directory}/{name}"
        if mode.startswith("04"):
            children.extend(tree(image, path))
            children.append(("rmdir", path))
        elif mode.startswith("10"):
            children.append(("rm", path))
        else:
            raise ValueError(f"Unexpected ext4 entry type: {path}, mode {mode}")
    if not children:
        raise ValueError(f"No entries in expected app directory {directory}")
    return children


def trim_filesystem(image, scratch):
    removals = []
    for app in APPS:
        removals.extend(tree(image, app))
        removals.append(("rmdir", app))
    removals.extend(("rm", path) for path in RECOVERY_FILES)

    blocks = set()
    for command, path in removals:
        if command == "rm":
            content = debug(image, f"blocks {path}").split()
            if not content:
                raise ValueError(f"No data blocks for {path}")
            blocks.update(map(int, content))
    commands = scratch / "remove.debugfs"
    commands.write_text("".join(f"{action} {path}\n" for action, path in removals))
    run("debugfs", "-w", "-f", commands, image, capture_output=True)
    for _, path in removals:
        if debug(image, f"stat {path}").strip():
            raise ValueError(f"Entry still present: {path}")

    # Zero discarded file contents only after confirming ext4 freed every block.
    checks = scratch / "test-blocks.debugfs"
    checks.write_text("".join(f"testb {block}\n" for block in sorted(blocks)))
    output = run("debugfs", "-f", checks, image, capture_output=True, text=True).stdout
    freed = {int(block) for block in re.findall(r"Block (\d+) not in use", output)}
    if blocks != freed:
        raise ValueError(f"{len(blocks - freed)} removed-file blocks are still in use")
    with image.open("r+b") as file:
        for block in sorted(freed):
            file.seek(block * BLOCK)
            file.write(bytes(BLOCK))
    print(f"Zeroed {len(freed)} freed file blocks", flush=True)


def ranges(transfer):
    lines = transfer.decode().splitlines()
    if lines[0] != "4" or lines[2:4] != ["0", "0"]:
        raise ValueError("Expected a full block v4 OTA with no stashes")
    new = []
    previous = 0
    write_blocks = 0
    covered = []
    for line in lines[4:]:
        action, encoded = line.split()
        count, *bounds = map(int, encoded.split(","))
        if count != len(bounds) or count % 2 or action not in ("new", "zero", "erase"):
            raise ValueError(f"Unexpected OTA transfer operation: {line}")
        for start, end in zip(bounds[::2], bounds[1::2]):
            if not 0 <= start < end <= SYSTEM_FS_BLOCKS:
                raise ValueError(f"Out-of-bounds OTA transfer: {line}")
            covered.append((start, end))
            if action != "erase":
                write_blocks += end - start
            if action == "new":
                if start < previous:
                    raise ValueError("Expected sorted non-overlapping new ranges")
                new.append((start, end))
                previous = end
    if write_blocks != int(lines[1]):
        raise ValueError("Transfer list new and zero block counts differ from the header")
    checked_end = 0
    for start, end in sorted(covered):
        if start != checked_end:
            raise ValueError("Full OTA ranges must cover SYSTEM without overlap")
        checked_end = end
    if checked_end != SYSTEM_FS_BLOCKS or checked_end * BLOCK > SYSTEM_BYTES:
        raise ValueError("Full OTA ranges do not match the SYSTEM filesystem geometry")
    return new


def transform_image(args, base, scratch):
    transfer = ranges(base.read(TRANSFER))
    packed = scratch / NEW_DATA
    image = scratch / "system.img"
    with base.open(NEW_DATA) as source, packed.open("wb") as target:
        shutil.copyfileobj(source, target)
    with image.open("wb") as target, subprocess.Popen(
        [args.brotli, "-d", "-c", packed], stdout=subprocess.PIPE
    ) as decoder:
        target.truncate(SYSTEM_FS_BLOCKS * BLOCK)
        for start, end in transfer:
            target.seek(start * BLOCK)
            expected = (end - start) * BLOCK
            chunk = decoder.stdout.read(expected)
            if len(chunk) != expected:
                raise ValueError("Truncated Brotli block stream")
            target.write(chunk)
        if decoder.stdout.read(1) or decoder.wait():
            raise ValueError("Unexpected data after the last block")

    run("e2fsck", "-fn", image)
    trim_filesystem(image, scratch)
    run("e2fsck", "-fn", image)

    # The stock full OTA already clears the gaps between new ranges.
    # Validate that filesystem changes did not write any bytes into those gaps.
    raw = scratch / "system.new.dat"
    with image.open("rb") as source, raw.open("wb") as target:
        previous = 0
        for start, end in [*transfer, (SYSTEM_FS_BLOCKS, SYSTEM_FS_BLOCKS)]:
            while previous < start:
                blocks = min(start - previous, 1024)
                gap = source.read(blocks * BLOCK)
                if gap != bytes(blocks * BLOCK):
                    raise ValueError("Modified filesystem data outside OTA new ranges")
                previous += blocks
            target.write(source.read((end - start) * BLOCK))
            previous = end
    run(args.brotli, "-q", "6", "-f", "-o", packed, raw)
    return packed, transfer, digest(image), digest(raw)


def local_offset(archive, entry):
    info = archive.getinfo(entry)
    if info.compress_type != zipfile.ZIP_STORED:
        raise ValueError(f"Metadata must be stored: {entry}")
    archive.fp.seek(info.header_offset + 26)
    name_size, extra_size = struct.unpack("<HH", archive.fp.read(4))
    return info.header_offset + 30 + name_size + extra_size


def property_files(path):
    with zipfile.ZipFile(path) as archive:
        return ",".join(f"{Path(entry).name}:{local_offset(archive, entry)}:"
                        f"{archive.getinfo(entry).file_size}" for entry in (METADATA, METADATA + ".pb"))


def write_zip(args, base, output, packed, properties):
    metadata = base.read(METADATA).decode()
    old_properties = re.search(r"^ota-property-files=(.*)$", metadata, re.M).group(1)
    properties = (properties or old_properties).ljust(len(old_properties))
    if len(properties) != len(old_properties):
        raise ValueError("OTA property offset exceeded its reserved space")
    old_devices = "klimtwifi,klimtltecan,klimtlte,"
    metadata = metadata.replace(old_properties, properties).replace(old_devices, "klimtwifi")
    protobuf = [args.protoc, f"--proto_path={args.proto.parent}", args.proto.name]
    schema = "build.tools.releasetools.OtaMetadata"
    decoded = run(*protobuf, f"--decode={schema}", input=base.read(METADATA + ".pb"),
                  stdout=subprocess.PIPE).stdout.decode()
    decoded = decoded.replace(old_properties, properties).replace(old_devices, "klimtwifi")
    encoded = run(*protobuf, f"--encode={schema}", input=decoded.encode(),
                  stdout=subprocess.PIPE).stdout

    updater = base.read(UPDATER).decode()
    if not updater.startswith("assert(") or "ui_print(" not in updater:
        raise ValueError("Unexpected stock updater script")
    updater = ('assert(getprop("ro.product.device") == "klimtwifi" || '
               'getprop("ro.build.product") == "klimtwifi" || '
               'abort("This package requires SM-T700 (klimtwifi)."));\n'
               'ui_print("LineageOS 21 Slim for SM-T700");\n'
               + updater[updater.index("ui_print("):])
    overrides = {METADATA: metadata.encode(), METADATA + ".pb": encoded, UPDATER: updater.encode()}
    with zipfile.ZipFile(output, "w") as target:
        for entry in base.namelist():
            if entry in ("recovery.img", CERTIFICATE):
                continue
            if entry in overrides:
                target.writestr(entry, overrides[entry])
            elif entry == NEW_DATA:
                target.write(packed, entry)
            else:
                with base.open(entry) as source, target.open(entry, "w") as sink:
                    shutil.copyfileobj(source, sink)


def sign(args, unsigned, signed):
    run("java", "-cp", args.signapk_classpath, "com.android.signapk.SignApk",
        "-w", args.cert, args.key, unsigned, signed)


def build(args):
    if args.output.exists():
        raise FileExistsError(args.output)
    if digest(args.base) != BASE_SHA256:
        raise ValueError("Input is not the verified 2026-10-07 klimtwifi build")
    with zipfile.ZipFile(args.base) as base, tempfile.TemporaryDirectory(
        prefix="slim-ota-", dir=args.scratch
    ) as work:
        scratch = Path(work)
        if base.getinfo("boot.img").file_size > BOOT_BYTES:
            raise ValueError("Base boot image does not fit BOOT")
        if base.read(CERTIFICATE) != args.cert.read_bytes():
            raise ValueError("Signing certificate differs from the base OTA certificate")
        packed, blocks, image_hash, stream_hash = transform_image(args, base, scratch)
        unsigned = scratch / "unsigned.zip"
        provisional = scratch / "provisional.zip"
        signed = scratch / "signed.zip"
        write_zip(args, base, unsigned, packed, None)
        sign(args, unsigned, provisional)
        properties = property_files(provisional)
        write_zip(args, base, unsigned, packed, properties)
        sign(args, unsigned, signed)
        if properties != property_files(signed):
            raise ValueError("Final ZIP property offsets differ from metadata")
        with zipfile.ZipFile(signed) as release:
            if release.testzip():
                raise ValueError("Final ZIP CRC verification failed")
            for entry in ("boot.img", TRANSFER, "system.patch.dat",
                          "META-INF/com/google/android/update-binary",
                          "install/bin/backuptool.sh", "install/bin/backuptool.functions"):
                if release.read(entry) != base.read(entry):
                    raise ValueError(f"Required stock entry changed: {entry}")
            if "recovery.img" in release.namelist():
                raise ValueError("Recovery image present in final ZIP")
        print(f"SYSTEM SHA-256: {image_hash}")
        print(f"New stream SHA-256: {stream_hash}")
        print(f"New blocks: {sum(end - start for start, end in blocks)}")
        print(f"{digest(signed)}  {args.output.name}")
        with signed.open("rb") as source, args.output.open("xb") as target:
            shutil.copyfileobj(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--brotli", default="brotli")
    parser.add_argument("--protoc", default="protoc")
    parser.add_argument("--proto", type=Path, required=True)
    parser.add_argument("--signapk-classpath", required=True)
    parser.add_argument("--cert", type=Path, required=True)
    parser.add_argument("--key", type=Path, required=True)
    parser.add_argument("--scratch", type=Path)
    build(parser.parse_args())


if __name__ == "__main__":
    main()
