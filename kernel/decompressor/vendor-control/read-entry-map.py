#!/usr/bin/env python3
"""Decode one SM-T700 translation-control report from a saved recovery console."""
import argparse
import re
from pathlib import Path


def translation(va, par):
    if par & 1:
        return f"fault status=0x{(par >> 1) & 0x3f:02x}"
    if par & 2:  # Supersection: PAR[23:16] contains address bits [39:32].
        address = ((par & 0xff0000) << 16) | (par & 0xff000000) | (va & 0xffffff)
    else:
        address = (par & 0xfffff000) | (va & 0xfff)
    return f"output=0x{address:08x}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("console", type=Path)
    parser.add_argument("run_id", help="hexadecimal ID embedded in the trial ramdisk")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]+", args.run_id):
        parser.error("expected a lowercase hexadecimal run ID")
    prefix = b"KLIMT-VENDOR " + args.run_id.encode() + b" ENTRY "
    reports = re.findall(re.escape(prefix) + rb"([^\r\n]*)", args.console.read_bytes())
    if len(reports) != 1 or not re.fullmatch(rb"[0-9a-f]{8}(?:,[0-9a-f]{8}){18}", reports[0]):
        parser.exit(1, "Expected one complete nineteen-word report from this run.\n")
    words = [int(field, 16) for field in reports[0].split(b",")]
    if words[0] != 1:
        reasons = {2: "outside SVC", 3: "long descriptors", 4: "record write denied",
                   5: "record outside executing page", 6: "MMU disabled"}
        parser.exit(1, f"Sampling skipped: {reasons.get(words[0], 'unknown status')}.\n")
    for label, word in zip(("CPSR", "SCTLR", "TTBCR", "TTBR0", "TTBR1", "DACR"), words[1:7]):
        print(f"{label} 0x{word:08x}")
    base, boot_params = words[7:9]
    probes = (
        ("entry read", base), ("boot params read", boot_params),
        ("tenth DTB first read", base + 0x59a250), ("tenth DTB last read", base + 0x5a7778),
        ("table first read", 0x20004000), ("table first write", 0x20004000),
        ("table last read", 0x20007fff), ("table last write", 0x20007fff),
        ("console read", 0x40000000), ("console write", 0x40000000),
    )
    for (label, va), par in zip(probes, words[9:]):
        va &= 0xffffffff
        print(f"{label}: VA=0x{va:08x} PAR=0x{par:08x} {translation(va, par)}")


if __name__ == "__main__":
    main()
