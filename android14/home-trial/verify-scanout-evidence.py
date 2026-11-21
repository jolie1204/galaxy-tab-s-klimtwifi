#!/usr/bin/env python3
import argparse
from collections import defaultdict
from datetime import datetime, timedelta
from pathlib import Path
import re

from PIL import Image

cli = argparse.ArgumentParser(description="Decode the Android scanout sample and pair nearby camera frames")
cli.add_argument("evidence", type=Path, help="Directory with the ACM logs and timestamped camera frames")
work = cli.parse_args().evidence.resolve()
serial = (work / "serial.private").read_text(errors="replace")
if "window=1,sf=1,boot=1" not in serial or (
    "KLIMT-HOME RESUMED com.android.launcher3/.uioverrides.QuickstepLauncher" not in serial
):
    raise SystemExit("Boot completion and Trebuchet resume have yet to be captured")

number = r"[0-9]+\.[0-9]+"
head = rf"^(\S+) kernel \[\s*({number})\] KLIMT-HOME DIAG 35 "
frame = re.compile(head + rf"HWC_FRAME u=({number}) fb=(\d+) size=(\d+x\d+) planes=(\d+) lit=(\d+)$")
pixels = re.compile(rf" HWC_PIXELS([01]) u=({number}) ([A-Za-z0-9_-]{{48}})$")
photo_name = re.compile(r"\d+s-(\d{6})\.jpg$")
halves = defaultdict(dict)
frames = {}
for line in (work / "acm-events.txt").read_text().splitlines():
    if match := frame.search(line):
        uptime = match[3]
        if match[5] in {"1600x2560", "2560x1600"} and 60 <= int(match[7]) <= 96:
            frames[uptime] = (match, line)
    elif match := pixels.search(line):
        halves[match[2]][int(match[1])] = match[3]

alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
camera = []
for path in (work / "frames").glob("*.jpg"):
    if match := photo_name.fullmatch(path.name):
        camera.append((datetime.strptime(match[1], "%H%M%S").time(), path))

matched = False
for uptime, (match, line) in frames.items():
    if set(halves[uptime]) != {0, 1}:
        continue
    time_logged = datetime.fromisoformat(match[1])
    sample_time = time_logged - timedelta(seconds=float(match[2]) - float(uptime))
    photos = {}
    for photo_time, path in camera:
        same_day = datetime.combine(sample_time.date(), photo_time, tzinfo=sample_time.tzinfo)
        for offset in (-1, 0, 1):
            photos[same_day + timedelta(days=offset)] = path
    before = max((moment for moment in photos if moment <= sample_time), default=None)
    after = min((moment for moment in photos if moment > sample_time), default=None)
    if before is None or after is None:
        continue
    if max(sample_time - before, after - sample_time) > timedelta(seconds=6):
        continue
    color_values = [alphabet.index(value) for value in halves[uptime][0] + halves[uptime][1]]
    if len(set(color_values)) < 2:
        continue
    screenshot = Image.new("RGB", (8, 12))
    screenshot.putdata([((bits >> 4) * 85, ((bits >> 2) & 3) * 85, (bits & 3) * 85)
                        for bits in color_values])
    target = work / f"android-hwc-scanout-{uptime}.png"
    screenshot.save(target)
    print(line)
    print(f"Expected local capture time {sample_time.isoformat(timespec='milliseconds')}")
    print(target)
    print(photos[before])
    print(photos[after])
    matched = True

if not matched:
    raise SystemExit("No complete scanout sample with nearby camera frames")
