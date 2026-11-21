#!/usr/bin/env python3
"""Change the one known compact Android bool in the SM-T700 test framework."""
import hashlib
from pathlib import Path
import re
import struct
import sys
import zipfile

original, output = map(Path, sys.argv[1:3])
expected = "e96cd37fbbfd89832ef55e91ff37919df1284aae41d7260d0b502fa16929901f"
if hashlib.file_digest(original.open("rb"), "sha256").hexdigest() != expected:
    raise SystemExit("Unexpected original framework-res.apk")
resource_id = 0x01110140
type_id = (resource_id >> 16) & 0xff
entry_id = resource_id & 0xffff

def chunks(data, start, end):
    while start < end:
        resource_type, header_size, size = struct.unpack_from("<HHI", data, start)
        if not 8 <= header_size <= size <= end - start:
            raise ValueError("Malformed Android resource table chunk")
        yield resource_type, start, header_size, size
        start += size

with zipfile.ZipFile(original) as source:
    resources = bytearray(source.read("resources.arsc"))
    [(kind, start, header, size)] = list(chunks(resources, 0, len(resources)))
    assert kind == 2 and start == 0
    candidates = []
    for kind, package, header, size in chunks(resources, header, size):
        if kind != 0x200 or struct.unpack_from("<I", resources, package + 8)[0] != 1:
            continue
        for kind, section, header, extent in chunks(resources, package + header, package + size):
            if kind != 0x201 or resources[section + 8] != type_id:
                continue
            count, entries_start = struct.unpack_from("<II", resources, section + 12)
            flags = resources[section + 9]
            offsets = section + header
            if flags == 1:
                sparse = resources[offsets:offsets + count * 4]
                matches = [offset for index, offset in struct.iter_unpack("<HH", sparse) if index == entry_id]
            else:
                assert flags == 2
                if entry_id >= count:
                    continue
                offset = struct.unpack_from("<H", resources, offsets + entry_id * 2)[0]
                matches = [] if offset == 0xffff else [offset]
            for offset in matches:
                entry = section + entries_start + offset * 4
                assert section + entries_start <= entry <= section + extent - 8
                _, entry_flags, value = struct.unpack_from("<HHI", resources, entry)
                assert entry_flags == 0x1208 and value == 0
                candidates.append(entry + 4)

    [offset] = candidates
    struct.pack_into("<I", resources, offset, 0xffffffff)
    with zipfile.ZipFile(output, "w") as target:
        for item in source.infolist():
            if re.fullmatch(r"META-INF/(MANIFEST\.MF|[^/]+\.(SF|RSA|DSA|EC))", item.filename, re.I):
                continue
            data = resources if item.filename == "resources.arsc" else source.read(item)
            target.writestr(item, data)
print(f"Set Android framework resource {resource_id:#010x}, compact boolean data at {offset}.")
