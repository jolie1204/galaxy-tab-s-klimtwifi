#!/usr/bin/env python3
"""Extract a unit's 2-nit S6E3HA1 gamma row from its stock kernel log."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("log", type=Path, help="Complete stock gamma_table output")
parser.add_argument("header", type=Path, help="Private generated header")
args = parser.parse_args()

rows = []
for line in args.log.read_text(errors="replace").splitlines():
    values = re.findall(r"0x([0-9a-fA-F]{2})\b", line)
    if len(values) == 34 and values[0].lower() == "ca":
        rows.append(values)

if len(rows) != 63:
    parser.error("expected exactly one 63-row stock S6E3HA1 gamma table")
values = [f"0x{value.lower()}" for value in rows[0]]
lines = [", ".join(values[start:start + 10]) for start in range(0, len(values), 10)]
with args.header.open("x") as output:
    output.write("\tstatic const u8 gamma_2nit[] = {\n")
    output.write("".join(f"\t\t{line},\n" for line in lines))
    output.write("\t};\n")
