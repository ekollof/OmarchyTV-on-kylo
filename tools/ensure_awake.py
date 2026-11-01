#!/usr/bin/env python3
"""Make sure the TV is awake, waking it with WoL if needed.

The TV suspends ~3 min after boot when it has no signal (HDMI1), which is
normal platform behaviour — see artifacts/reports and TODO.md. This helper
checks liveness, sends a magic packet if the set is in quick standby, then
waits until sshd is reachable.

Usage:
    tools/ensure_awake.py [--host IP] [--mac MAC] [--timeout SEC] [--quiet]

Exit code 0 when the host is reachable, 1 on timeout.
"""
import argparse
import socket
import sys
import time

DEFAULT_HOST = "192.168.178.87"
DEFAULT_MAC = "70:54:b4:fe:70:c6"
# tvssh.sh needs SSH, not merely a responding vendor service during early boot.
PROBE_PORTS = (22,)


def reachable(host: str, timeout: float = 1.0) -> bool:
    for port in PROBE_PORTS:
        s = socket.socket()
        s.settimeout(timeout)
        try:
            s.connect((host, port))
            return True
        except OSError:
            pass
        finally:
            s.close()
    return False


def send_wol(mac: str) -> None:
    payload = b"\xff" * 6 + bytes.fromhex(mac.replace(":", "")) * 16
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    for _ in range(5):
        for port in (9, 7):
            for dst in ("192.168.178.255", "255.255.255.255"):
                s.sendto(payload, (dst, port))
        time.sleep(0.2)
    s.close()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--mac", default=DEFAULT_MAC)
    ap.add_argument("--timeout", type=float, default=60.0)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    def say(msg: str) -> None:
        if not args.quiet:
            print(msg, file=sys.stderr)

    if reachable(args.host):
        say(f"{args.host} already awake")
        return 0

    say(f"{args.host} is asleep/unreachable — sending magic packet for {args.mac}")
    send_wol(args.mac)

    deadline = time.time() + args.timeout
    while time.time() < deadline:
        if reachable(args.host):
            say(f"{args.host} awake")
            return 0
        time.sleep(1)

    say(f"{args.host} did not come back within {args.timeout:.0f}s")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
