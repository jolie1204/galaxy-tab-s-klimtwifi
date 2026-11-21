#!/usr/bin/env python3
"""Verify SDK extension properties and ART classpath delivery in a trial 27 log."""

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

    def exited(service, identity):
        started = re.search(rf"SVC_EXEC service '{service}' pid (\d+) \({identity}\)", log)
        require(started, f"expected execution of {service}")
        status = one(f"Service '{service}' (pid {started[1]}) exited with status 0")
        require(started.start() < status, f"{service} exited before it started")
        return status

    config = "init: linkerconfig generated /linkerconfig with mounted APEX modules info"
    require(log.count(config) == 2, "expected both linker configurations")
    sdk_rc = one("Parsing file /apex/com.android.sdkext/etc/derive_sdk.rc")
    classpath_rc = one("Parsing file /apex/com.android.sdkext/etc/derive_classpath.rc")
    factory = one(f"KLIMT-FACTORY PASS run={run_id} mounts=31 tmpfs=1 config=1")
    before = one(f"KLIMT-CP BEFORE {run_id} api=34 r=unset s=unset t=unset u=unset ad=unset")
    clean = one("KLIMT-CP clean=1 absent=1 base=1")
    sdk_exit = exited("derive_sdk", r"uid 9999 gid 9999\+0 context default")
    sdk = one(f"KLIMT-CP SDK {run_id} api=34 r=11 s=11 t=11 u=11 ad=11")
    recovery = one("KLIMT-FACTORY RECOVERY reason=verdict")
    restart = one("Restarting system with command 'recovery'")
    require(max(sdk_rc, classpath_rc) < log.rindex(config) < factory < before < clean < sdk_exit < sdk,
            "SDK boot events out of order")

    if negative:
        skip = one(f"KLIMT-CP SKIPPED run={run_id} absent=1 base=1")
        require(sdk < skip < recovery < restart, "negative boot events out of order")
        for marker in ("KLIMT-CP PASS", "KLIMT-CP END", "starting service 'derive_classpath'",
                       "starting service 'klimt-classpath-env'"):
            require(marker not in log, f"unexpected negative event {marker!r}")
        return

    classpath_exit = exited("derive_classpath", r"uid 1000 gid 1000\+1 context default")
    shell_exit = exited("klimt-classpath-env", r"uid 2000 gid 2000\+0 context u:r:shell:s0")
    digest = "0de7b2d619babd1ad9e9ce64548dff15c3f8e16a63bcdf4c2ec969efb17696e1"
    inherited = one(f"KLIMT-CP END {run_id} env={digest}")
    verdict = one(f"KLIMT-CP PASS run={run_id} bytes=3887 sha256={digest} "
                  "parsed=1 env_match=1 enforcing=1")
    for name, count in (("BOOTCLASSPATH", 39), ("DEX2OATBOOTCLASSPATH", 14),
                        ("SYSTEMSERVERCLASSPATH", 13), ("STANDALONE_SYSTEMSERVER_JARS", 7)):
        require(inherited < one(f"KLIMT-CP export {name} jars={count}") < verdict,
                f"expected {count} accessible entries on {name}")
    require(sdk < classpath_exit < shell_exit < inherited < verdict < recovery < restart,
            "classpath boot events out of order")
    require("KLIMT-CP FAIL" not in log and "KLIMT-CP missing_jar" not in log, "classpath check failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("run_id")
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{16}", args.run_id), "invalid run ID")
    verify(args.log.read_text(errors="replace"), args.run_id, args.negative)
    description = "SDK and empty classpath control" if args.negative else "SDK and four exported classpaths"
    print(f"{args.log.name}: verified {description}")


if __name__ == "__main__":
    main()
