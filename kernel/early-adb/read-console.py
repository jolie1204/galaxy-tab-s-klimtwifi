#!/usr/bin/env python3
"""Print the SM-T700 trial checkpoints present in a saved recovery console."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("previous_console", type=Path)
args = parser.parse_args()
log = args.previous_console.read_bytes()
patterns = (
    rb"(?m)^KLIMT-(?:612-(?:ENTRY|PRECACHE|TABLEBUILT|PTREADY|TTBRLIVE|POSTCACHE|PREINFLATE|INFLATED)|VENDOR-ENTRY) [0-9a-f]{16}(?=\r?$)",
    rb"Linux version 6\.12\.118-klimt-(?:early1|adbreturn1)(?=[ (])",
    rb"KLIMT-EARLY1 PID1 before USB 6\.12\.118-klimt-early1\b",
    rb"KLIMT-EARLY1 USB setup pid=\d+ 6\.12\.118-klimt-adbreturn1\b",
    rb"KLIMT-EARLY1 adbd started pid=\d+",
    rb"KLIMT-EARLY1 FunctionFS ready",
    rb"KLIMT-EARLY1 waiting for tablet USB controller",
    rb"KLIMT-EARLY1 UDC bind failed; retrying",
    rb"KLIMT-EARLY1 UDC bound",
    rb"KLIMT-RETURN authenticated shell requested recovery",
    rb"KLIMT-RETURN USB trial deadline reached",
    rb"KLIMT-RETURN waiting for recovery mode drivers",
    rb"KLIMT-RETURN requesting recovery",
    rb"KLIMT-RETURN reboot syscall returned",
)
found = False
for pattern in patterns:
    for line in dict.fromkeys(match[0] for match in re.finditer(pattern, log)):
        print(line.decode("ascii"))
        found = True
if not found:
    print("No trial checkpoint found in this file.")
    raise SystemExit(1)
