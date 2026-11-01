#!/usr/bin/env python3
"""Omarchy status endpoint for the TV launcher (the "omarchy · pc" / "ai credits" panels).

Serves GET /v1/status as the JSON OmarchyTV expects:

    { host, kernel, cpu, ram, gpu, deploy, theme, ai: [ {name, plan, used} ] }
    cpu/ram  = percent (0-100)
    ai       = optional list; upstream falls back to dollar strings when absent

CORS is wide open (the TV page runs on http://127.0.0.1:8181 and fetches this cross-origin).

    tools/pcstatus.py --port 8765
    tools/pcstatus.py --bind 0.0.0.0 --port 8765     # reachable from the TV

No dependencies beyond the standard library.
"""
import argparse
import glob
import json
import os
import platform
import shutil
import socket
import subprocess
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

START = time.time()
_last_cpu = None


def cpu_percent():
    """CPU load since the previous call (falls back to 1-minute loadavg)."""
    global _last_cpu
    try:
        with open("/proc/stat") as f:
            parts = [int(x) for x in f.readline().split()[1:]]
        idle, total = parts[3] + parts[4], sum(parts)
        if _last_cpu:
            d_total = total - _last_cpu[1]
            d_idle = idle - _last_cpu[0]
            _last_cpu = (idle, total)
            if d_total > 0:
                return max(0, min(100, round(100 * (d_total - d_idle) / d_total)))
        _last_cpu = (idle, total)
    except Exception:
        pass
    try:
        return min(100, round(os.getloadavg()[0] / (os.cpu_count() or 1) * 100))
    except Exception:
        return 0


def ram_percent():
    try:
        info = {}
        with open("/proc/meminfo") as f:
            for line in f:
                k, v = line.split(":", 1)
                info[k] = int(v.split()[0])
        total = info["MemTotal"]
        avail = info.get("MemAvailable", info.get("MemFree", 0))
        return max(0, min(100, round(100 * (total - avail) / total)))
    except Exception:
        return 0


def gpu_percent():
    """AMD/Intel: read the busiest GPU sysfs counter; otherwise None."""
    best = None
    try:
        for path in glob.glob("/sys/class/drm/card[0-9]/device/gpu_busy_percent"):
            with open(path) as f:
                val = int(f.read().strip())
            best = val if best is None else max(best, val)
    except Exception:
        pass
    return best


def now_playing():
    if not shutil.which("playerctl"):
        return None
    try:
        out = subprocess.run(["playerctl", "metadata", "--format",
                              "{{artist}} — {{title}}"],
                             capture_output=True, text=True, timeout=1.5).stdout.strip()
        return out or None
    except Exception:
        return None


def ai_rows():
    """Optional: read `claude`/`codex` usage if the user exposes it. Keep it honest: [] = unknown."""
    return []


def status():
    gpu = gpu_percent()
    return {
        "host": socket.gethostname(),
        "kernel": platform.release(),
        "cpu": cpu_percent(),
        "ram": ram_percent(),
        "gpu": (str(gpu) + "%") if gpu is not None else None,
        "deploy": f"up {int(time.time() - START)}s",
        "media": now_playing(),
        "ai": ai_rows(),
        "ts": int(time.time()),
    }


class Handler(BaseHTTPRequestHandler):
    server_version = "omarchy-pcstatus/1.0"

    def _send(self, code, body, ctype="application/json"):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?")[0].rstrip("/") or "/"
        if path in ("/v1/status", "/status"):
            self._send(200, json.dumps(status()))
        elif path == "/":
            self._send(200, "omarchy pcstatus: GET /v1/status\n", "text/plain")
        else:
            self._send(404, json.dumps({"error": "not found"}))

    def do_OPTIONS(self):          # CORS preflight (not needed for a simple GET, harmless)
        self._send(204, "")

    def log_message(self, fmt, *args):
        if os.environ.get("PCSTATUS_VERBOSE"):
            super().log_message(fmt, *args)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()
    srv = ThreadingHTTPServer((args.bind, args.port), Handler)
    print(f"pcstatus on http://{args.bind}:{args.port}/v1/status  (host {socket.gethostname()})", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
