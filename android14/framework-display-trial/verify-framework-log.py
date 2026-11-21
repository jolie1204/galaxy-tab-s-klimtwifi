#!/usr/bin/env python3
"""Read the framework and graphics dependency trial verdict."""

import argparse
from pathlib import Path
import re


def verify(log, run_id, case):
    display = (
        f"KLIMT-DISPLAY {run_id} hwsm=true hwc=stopped sf=stopped "
        "binder=activity=0,package=0,window=0,sf=0"
    )
    recovery = "reboot: Restarting system with command 'recovery'"
    expected = [recovery]
    if case == "skipped":
        expected.insert(0, f"KLIMT-ZYGOTE SKIP {run_id}")
        for service in ("installd", "vendor.hwcomposer-2-1", "surfaceflinger", "zygote"):
            if f"starting service '{service}'" in log:
                raise ValueError(f"skipped boot started {service}")
        if display in log:
            raise ValueError("skipped boot printed a display verdict")
    else:
        expected[:0] = [
            display,
            "KLIMT-DISPLAY HWC failed to open framebuffer",
            "KLIMT-DISPLAY SF eglInitialize(",
            "KLIMT-FRAMEWORK INSTALLD state=running",
        ]
        if not re.search(
            r"Service 'surfaceflinger' \(pid \d+\) received signal 11", log
        ) or not re.search(
            r"/system/lib/libEGL\.so \(android::egl_display_t::initialize", log
        ):
            raise ValueError("missing SurfaceFlinger EGL crash evidence")
        if not re.search(r"KLIMT-DISPLAY SF [^\n]+\(EGL_NOT_INITIALIZED\)", log):
            raise ValueError("missing EGL initialization error")
        if case == "physical":
            expected.insert(-1, f"KLIMT-ZYGOTE END {run_id} log=server=entered,fork=1")
            if not re.search(
                rf"KLIMT-ZYGOTE checked run={run_id} zygote=[1-9]\d* "
                r"system_server=[1-9]\d* enforcing=1",
                log,
            ):
                raise ValueError("missing live enforcing framework process evidence")

    cursor = 0
    for line in expected:
        match = log.find(line, cursor)
        if match == -1:
            raise ValueError(f"missing or out-of-order evidence: {line}")
        cursor = match + len(line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=("virtual", "skipped", "physical"))
    parser.add_argument("run_id")
    parser.add_argument("console", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[a-f0-9]{16}", args.run_id):
        parser.error("run ID must be 16 lowercase hexadecimal characters")
    try:
        verify(args.console.read_text(errors="replace"), args.run_id, args.case)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"{args.case} framework and graphics evidence verified for {args.run_id}")


if __name__ == "__main__":
    main()
