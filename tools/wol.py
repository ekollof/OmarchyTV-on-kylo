#!/usr/bin/env python3
"""Send a Wake-on-LAN magic packet.

Usage: wol.py [MAC] [broadcast]
Default MAC is the TV: 70:54:b4:fe:70:c6
"""
import socket
import sys
import time

DEFAULT_MAC = "70:54:b4:fe:70:c6"
DEFAULT_BCAST = "192.168.178.255"


def main() -> int:
    mac = (sys.argv[1] if len(sys.argv) > 1 else DEFAULT_MAC).replace(":", "").replace("-", "")
    bcast = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_BCAST
    if len(mac) != 12:
        print("bad MAC", file=sys.stderr)
        return 2
    payload = b"\xff" * 6 + bytes.fromhex(mac) * 16
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    for _ in range(5):
        for port in (9, 7):
            for dst in (bcast, "255.255.255.255"):
                s.sendto(payload, (dst, port))
        time.sleep(0.2)
    s.close()
    print(f"sent magic packets for {mac} via {bcast} (udp 9/7)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
