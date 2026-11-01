#!/usr/bin/env python3
"""Minimal openaurora control client: client.py <socket> <command> [...]"""
import socket
import sys
import time


def one(path: str, line: str) -> str:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3.0)
    s.connect(path)
    s.sendall((line + "\n").encode())
    out = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            out += chunk
            # the server replies once per command; short pause then stop
            s.settimeout(0.25)
    except socket.timeout:
        pass
    finally:
        s.close()
    return out.decode(errors="replace").strip()


if __name__ == "__main__":
    path = sys.argv[1]
    for line in sys.argv[2:]:
        print(one(path, line))
        time.sleep(0.05)
