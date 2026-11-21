#!/usr/bin/env python3
"""Verify a trial 25 kernel console capture with the packaged run ID."""

import argparse
import re
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(log, run_id, negative):
    def position(message):
        require(log.count(message) == 1, f"expected one occurrence of {message!r}")
        return log.index(message)

    start = position(f"KLIMT-BINDER BEGIN {run_id} cold=true ready=unset")
    linker = position("init: linkerconfig generated /linkerconfig with mounted APEX modules info")
    require(linker < start, "linker configuration must precede the trial")
    restart = position("Restarting system with command 'recovery'")
    calls = re.compile(r"KLIMT-BINDER call id=(\d+) client=(\d+) manager=(\d+) "
                       r"code=(\d+) uid=(\d+) shell_domain=([01])")

    if negative:
        client = position("SVC_EXEC service 'klimt-binder-client' pid ")
        timeout = position("KLIMT-BINDER RECOVERY reason=timeout")
        require(start < client < timeout < restart, "negative control events out of order")
        for prefix in ("KLIMT-BINDER call ", "KLIMT-BINDER reply ", "KLIMT-BINDER delivered_",
                       "KLIMT-BINDER SETUP_OK", "KLIMT-BINDER END", "starting service 'servicemanager'",
                       "Service 'klimt-binder-client' (pid "):
            require(prefix not in log, f"unexpected negative control event {prefix!r}")
        return []

    manager = re.search(r"started service 'servicemanager' has pid (\d+)", log)
    require(manager is not None, "Android did not report a service manager PID")
    clients = list(re.finditer(r"SVC_EXEC service 'klimt-binder-client' pid (\d+) "
                               r"\(uid 2000 gid 2000\+0 context u:r:shell:s0\)", log))
    require(len(clients) == 1, "expected one shell probe with the configured Android UID and domain")
    exit_status = position(f"Service 'klimt-binder-client' (pid {clients[0][1]}) exited with status 0")
    end = position(f"KLIMT-BINDER END {run_id} ready=true client=verified")
    require("KLIMT-BINDER missing_context_manager" not in log, "probe raced the context manager")
    for device in ("binder", "hwbinder", "vndbinder"):
        require(re.search(rf"KLIMT-BINDER node={device} dev=\d+:\d+ valid=1", log), device)
    setup = position(f"KLIMT-BINDER SETUP_OK run={run_id} devices=1 init_sequence=1 enforcing=1")
    recovery = position("KLIMT-BINDER RECOVERY reason=verdict")
    require(start < manager.start() < clients[0].start() < exit_status < end < setup < recovery < restart,
            "positive trial events out of order")

    exchanges = []
    for call in calls.finditer(log):
        call_id, client_pid, manager_pid, code, uid, domain = call.groups()
        require(manager_pid == manager[1] and uid == "2000" and domain == "1", "unexpected Binder sender")
        delivery = position(f"KLIMT-BINDER delivered_call id={call_id} manager={manager_pid} client={client_pid}")
        reply = re.search(
            rf"KLIMT-BINDER reply id=(\d+) call={call_id} manager={manager_pid} client={client_pid} "
            r"bytes=(\d+) manager_domain=1", log)
        require(reply is not None, f"missing service manager reply to transaction {call_id}")
        require(int(reply[2]) > 0 or int(code) == 1599098439, "empty non-ping reply")
        receipt = position(f"KLIMT-BINDER delivered_reply id={reply[1]} client={client_pid} bytes={reply[2]}")
        require(clients[0].start() < call.start() < delivery < reply.start() < receipt < exit_status,
                f"transaction {call_id} did not complete in order")
        exchanges.append((int(code), int(client_pid), int(call_id), int(reply[1])))

    # Android 14 IServiceManager.aidl assigns checkService code 2 and listServices code 4.
    require(sum(code == 2 for code, *_ in exchanges) == 3, "expected present and absent lookups")
    require(sum(code == 4 for code, *_ in exchanges) == 1, "expected one service list request")
    return exchanges


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("run_id")
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{16}", args.run_id), "invalid run ID")
    log = args.log.read_text(errors="replace")
    transactions = verify(log, args.run_id, args.negative)
    print(f"{args.log.name}: verified {len(transactions)} synchronous Binder exchanges")
    for code, client, call, reply in transactions:
        print(f"  code={code} client={client} call={call} reply={reply}")


if __name__ == "__main__":
    main()
