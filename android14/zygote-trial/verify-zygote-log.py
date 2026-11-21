#!/usr/bin/env python3
"""Verify the ART cache and first framework checkpoints for trial 29."""

import argparse
import re
from pathlib import Path


def require(ok, description):
    if not ok:
        raise ValueError(description)


def verify(log, run_id, negative):
    def one(marker):
        require(log.count(marker) == 1, f"expected one {marker!r}")
        return log.index(marker)

    def exit_status(service, status):
        found = re.search(rf"Service '{service}' \(pid (\d+)\) exited with status {status}\b", log)
        require(found, f"expected {service} exit {status}")
        return found

    check = one(f"KLIMT-ART END {run_id} log=version=1,boot=ok,server=ok")
    cache_build = exit_status("klimt-cache", 80)
    cache_check = exit_status("klimt-cache-recheck", 0)
    cache = one(f"KLIMT-ZYGOTE CACHE {run_id}")
    present = one("KLIMT-ZYGOTE cache_present=1 prior_art=1 error=0")
    recovery = one("KLIMT-FACTORY RECOVERY reason=verdict")
    restart = one("Restarting system with command 'recovery'")
    require(check < cache_build.start() < cache_check.start() < cache < present < recovery < restart,
            "ART cache events out of order")

    if negative:
        skip = one(f"KLIMT-ZYGOTE SKIP {run_id}")
        checked = one(f"KLIMT-ZYGOTE checked run={run_id} zygote=0 system_server=0 enforcing=1")
        require(present < skip < checked < recovery, "negative control order")
        require("KLIMT-ART exec=zygote" not in log and "starting service 'zygote'" not in log,
                "Zygote ran in negative control")
        return "ART cache, Zygote skipped"

    zygote = re.search(r"KLIMT-ART exec=zygote pid=(\d+) domain=u:r:zygote:s0 enforcing=1", log)
    require(zygote and int(zygote[1]) > 1, "expected enforcing Zygote exec")
    collector_done = exit_status("klimt-zygote-log", 0)
    summary = re.search(rf"KLIMT-ZYGOTE END {run_id} log=server=(entered|unconfirmed),fork=([01])", log)
    observed = re.search(rf"KLIMT-ZYGOTE checked run={run_id} zygote={zygote[1]} "
                         r"system_server=(\d+) enforcing=1", log)
    require(summary and observed, "expected completed framework observation")
    require(present < zygote.start() < collector_done.start() < summary.start() <
            observed.start() < recovery, "framework events out of order")
    result = {"zygote": int(zygote[1]), "system_server": int(observed[1]),
              "java_entry": summary[1] == "entered", "fork_log": summary[2] == "1"}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("run_id")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--expect-server", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{16}", args.run_id), "invalid run ID")
    result = verify(args.log.read_text(errors="replace"), args.run_id, args.negative)
    if args.expect_server:
        require(not args.negative and result["java_entry"] and result["system_server"] > 1,
                "no live Java system_server")
    print(f"{args.log.name}: {result}")


if __name__ == "__main__":
    main()
