#!/usr/bin/env python3
"""Build the SM-T700 Linux 6.12 diagnostic copy of Android 14's cgroups.json."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("stock", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
if args.output.exists() and args.stock.samefile(args.output):
    parser.error("the output must be a separate file")
profile = json.loads(args.stock.read_text())
rows = profile["Cgroups"]
legacy = [row for row in rows if row["Controller"] == "schedtune"]
if len(legacy) != 1 or legacy[0].get("Optional") is not True or legacy[0]["Path"] != "/dev/stune":
    parser.error("expected one optional schedtune entry at /dev/stune")
rows.remove(legacy[0])
args.output.write_text(json.dumps(profile, indent=2) + "\n")
