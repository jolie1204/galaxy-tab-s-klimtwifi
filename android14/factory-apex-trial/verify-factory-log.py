#!/usr/bin/env python3
"""Verify the complete factory APEX set in a trial 26 console capture."""

import argparse
import re
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(log, run_id, negative, inventory):
    def one(marker):
        require(log.count(marker) == 1, f"expected one occurrence of {marker!r}")
        return log.index(marker)

    bootstrap = one(f"KLIMT-FACTORY BOOTSTRAP {run_id} status=unset")
    default = one(f"KLIMT-FACTORY DEFAULT {run_id} status=unset")
    clean = one(f"KLIMT-FACTORY clean_default=1 tmpfs=1 absent={len(inventory)}")
    restart = one("Restarting system with command 'recovery'")

    if negative:
        skip = one(f"KLIMT-FACTORY SKIP {run_id} status=unset")
        verdict = one(f"KLIMT-FACTORY SKIPPED run={run_id} absent={len(inventory)} actor=0")
        recovery = one("KLIMT-FACTORY RECOVERY reason=verdict")
        require(bootstrap < default < clean < skip < verdict < recovery < restart,
                "negative events out of order")
        for marker in ("KLIMT-FACTORY END", "KLIMT-FACTORY PASS", "KLIMT-FACTORY module=",
                       "init: starting service 'apexd'", "apexd: Marking APEXd as activated"):
            require(marker not in log, f"unexpected negative event {marker!r}")
        return 0

    start = one("init: starting service 'apexd'")
    activate = one("apexd: Marking APEXd as activated")
    actor = re.search(r"KLIMT-FACTORY apexd pid=(\d+) default_ns=1 enforcing=1", log)
    require(actor and int(actor[1]) > 1, "apexd domain and default mount namespace")
    require(f"started service 'apexd' has pid {actor[1]}" in log, "apexd PID correlation")
    end = one(f"KLIMT-FACTORY END {run_id} status=activated service=running")
    verdict = one(f"KLIMT-FACTORY PASS run={run_id} mounts={len(inventory)} "
                  f"tmpfs=1 config=1 actor={actor[1]} enforcing=1")
    recovery = one("KLIMT-FACTORY RECOVERY reason=verdict")
    config = "init: linkerconfig generated /linkerconfig with mounted APEX modules info"
    require(log.count(config) == 2, "bootstrap and default linker configs")
    require(bootstrap < default < clean < start < activate < actor.start() <
            log.rindex(config) < end < verdict < recovery < restart, "positive events out of order")

    mounted = re.findall(
        r"apexd: Successfully mounted package (\S+) on /apex/([\w.]+)@(\d+) ", log[start:activate])
    require(len(mounted) == len(inventory), "factory mount count")
    require({(name, version, path) for path, name, version in mounted} == inventory,
            "factory package name, version and installed path")
    observed = re.findall(r"KLIMT-FACTORY module=([\w.]+) dev=7:(\d+) valid=1", log[end:verdict])
    require(len(observed) == len(inventory), "default namespace mount count")
    require(len({minor for _, minor in observed}) == len(inventory), "unique read-only loop devices")
    require({"com.android."+name for name, _ in observed} == {name for name, _, _ in inventory},
            "default namespace module identity")
    require("Failed to activate packages" not in log and "cannot emit apex info list" not in log,
            "apexd activation or inventory error")
    return len(observed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("run_id")
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{16}", args.run_id), "invalid run ID")
    inventory = {tuple(line.split()) for line in
                 Path(__file__).with_name("installed-modules.txt").read_text().splitlines()}
    require(len(inventory) == 31, "expected the trial 26 installed package inventory")
    count = verify(args.log.read_text(errors="replace"), args.run_id, args.negative, inventory)
    print(f"{args.log.name}: verified {count} factory APEX mounts in the default namespace")


if __name__ == "__main__":
    main()
