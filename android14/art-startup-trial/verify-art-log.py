#!/usr/bin/env python3
"""Verify the ART feature flags, artifact-check process and recovery in trial 28."""

import argparse
import re
from pathlib import Path


def require(ok, description):
    if not ok:
        raise ValueError(description)


def verify(log, run_id, negative, expect_uffd):
    def one(marker):
        require(log.count(marker) == 1, f"expected one {marker!r}")
        return log.index(marker)

    def actor(service, domain):
        match = re.search(
            rf"KLIMT-ART exec={service} pid=(\d+) domain={re.escape(domain)} enforcing=1", log)
        require(match and int(match[1]) > 1, f"expected enforcing exec for {service}")
        return match

    classpath = one(f"KLIMT-CP PASS run={run_id} bytes=3887 "
                    "sha256=0de7b2d619babd1ad9e9ce64548dff15c3f8e16a63bcdf4c2ec969efb17696e1")
    imported = one("Parsing file /apex/com.android.art/etc/init.rc")
    before = one(f"KLIMT-ART BEFORE {run_id} profile=unset stats=unset zygote=zygote32")
    clean = one("KLIMT-ART clean=1 cp=1 cache_absent=1")
    art = actor("art_boot", "u:r:art_boot:s0")
    art_done = one(f"Service 'art_boot' (pid {art[1]}) exited with status 0")
    flags = one(f"KLIMT-ART FLAGS {run_id} profile=true stats=true")
    recovery = one("KLIMT-FACTORY RECOVERY reason=verdict")
    restart = one("Restarting system with command 'recovery'")
    require(imported < classpath < before < clean < art.start() < art_done < flags,
            "ART initialization events out of order")
    for service in ("odsign", "zygote", "zygote_secondary", "dex2oat"):
        require(f"starting service '{service}'" not in log, f"unexpected service {service}")

    if negative:
        skip = one(f"KLIMT-ART SKIP {run_id}")
        verdict = one(f"KLIMT-ART checked run={run_id} art_pid={art[1]} "
                      "odr_pid=0 flags=1 ram=1 cache_absent=1 enforcing=1")
        require(flags < skip < verdict < recovery < restart, "negative events out of order")
        require("KLIMT-ART exec=odrefresh" not in log and "KLIMT-ART END" not in log,
                "artifact check ran in the negative control")
        return "ART feature flags and skipped artifact check"

    odr = actor("odrefresh", "u:r:odrefresh:s0")
    odr_done = one(f"Service 'klimt-odrefresh' (pid {odr[1]}) exited with status 79")
    collector = re.search(r"SVC_EXEC service 'klimt-art-log' pid (\d+) .*u:r:shell:s0", log)
    require(collector, "expected shell log collector")
    collector_done = one(f"Service 'klimt-art-log' (pid {collector[1]}) exited with status 0")
    expected_summary = ("version=1,boot=ok,server=ok" if expect_uffd else
                        "version=1,boot=system_ineligible,server=unchecked")
    summary = one(f"KLIMT-ART END {run_id} log={expected_summary}")
    system = one(f"KLIMT-ART SYSTEM {run_id} ")
    cache = one(f"KLIMT-ART CACHE {run_id} ")
    mismatch = "Userfaultfd GC check failed" in log[system:cache]
    require(mismatch != expect_uffd, "unexpected userfaultfd precondition result")
    verdict = one(f"KLIMT-ART checked run={run_id} art_pid={art[1]} odr_pid={odr[1]} "
                  "flags=1 ram=1 cache_absent=1 enforcing=1")
    require(flags < odr.start() < odr_done < collector.start() < collector_done <
            system < cache < summary < verdict < recovery < restart,
            "artifact check events out of order")
    return f"ART feature flags and odrefresh exit 79 ({expected_summary})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("run_id")
    control = parser.add_mutually_exclusive_group()
    control.add_argument("--negative", action="store_true")
    control.add_argument("--expect-uffd", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{16}", args.run_id), "invalid run ID")
    description = verify(args.log.read_text(errors="replace"), args.run_id, args.negative, args.expect_uffd)
    print(f"{args.log.name}: verified {description}")


if __name__ == "__main__":
    main()
