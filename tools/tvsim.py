#!/usr/bin/env python3
"""tvsim - offline stand-in for the TV's control plane, so the launcher can be tested without the set.

It emulates the pieces our launcher actually talks to, and drives a real browser:

    launcher/omarchy  ->  served on :8181 (static + Python shims for /cgi-bin/*)
    DIAL              ->  :56789 / :56790  (dd.xml, POST /apps/SmartCenter)
    state feed        ->  ws://127.0.0.1:7681/  (pushes <tv_state value='...'/>)
    screen + remote   ->  headless Chromium over CDP at 1280x720 CSS / DPR 1.5
                          (a screenshot is 1920x1080, like /dev/fb0 on the TV)

Everything is logged to stdout, so a session doubles as a transcript.

    tools/tvsim.py                      # serve + browser, keep running
    tools/tvsim.py --no-browser         # only the HTTP/WS side (drive it from a real browser)
    tools/tvsim.py --headful            # show the window (handy while tweaking the UI)

Control API (the same host:8181):
    GET /sim/key?code=1019              # remote key -> DOM key (TV-control keys close the page)
    GET /sim/load?url=...               # what `browserseturl` does
    GET /sim/shot?path=/tmp/shot.png    # capture the screen
    GET /sim/state                      # PORTAL / SOURCE / ...

CLI: tools/tvsimctl.sh key|load|shot|state
"""
import argparse
import base64
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.join(REPO, "launcher")
STATE = {"tv": "SOURCE", "last_key": None, "alive": 0, "boot_ok": False}
LOCK = threading.Lock()
CDP = {"ws": None, "seq": 0}
WS_CLIENTS = []

# DIAL key code -> (DOM key, windows virtual key code)
KEYS = {
    1000: ("0", 48), 1001: ("1", 49), 1002: ("2", 50), 1003: ("3", 51), 1004: ("4", 52),
    1005: ("5", 53), 1006: ("6", 54), 1007: ("7", 55), 1008: ("8", 56), 1009: ("9", 57),
    1010: ("Escape", 27), 1037: ("Escape", 27),
    1011: ("w", 87), 1012: ("Power", 0),
    1013: ("AudioVolumeMute", 173),
    1016: ("AudioVolumeUp", 175), 1017: ("AudioVolumeDown", 174),
    1018: ("i", 73),
    1019: ("ArrowDown", 40), 1020: ("ArrowUp", 38), 1021: ("ArrowLeft", 37), 1022: ("ArrowRight", 39),
    1024: ("MediaStop", 178), 1025: ("MediaPlayPause", 179),
    1048: ("ContextMenu", 93),      # MENU: on the TV this leaves the page
    1051: ("r", 82),
    1053: ("Enter", 13),            # OK
    1056: ("s", 83),                # source/AUX: on the TV this leaves the page
    1057: ("m", 77), 1064: ("n", 78),
}
# keys the TV itself consumes (they close the browser page)
TV_CONTROL = {1012, 1037, 1048, 1056, 1010}


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


# ---------------------------------------------------------------- state feed (ws://:7681)
def ws_frame(payload: str) -> bytes:
    data = payload.encode()
    head = b"\x81"
    n = len(data)
    if n < 126:
        head += bytes([n])
    elif n < 65536:
        head += bytes([126]) + n.to_bytes(2, "big")
    else:
        head += bytes([127]) + n.to_bytes(8, "big")
    return head + data


def push_state(state):
    STATE["tv"] = state
    msg = f"<tv_state value='{state}'/>\n"
    dead = []
    for c in list(WS_CLIENTS):
        try:
            c.sendall(ws_frame(msg))
        except Exception:
            dead.append(c)
    for c in dead:
        WS_CLIENTS.remove(c)


class StateWS(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        if "websocket" not in (self.headers.get("Upgrade", "") + self.headers.get("Connection", "")).lower():
            self.send_error(400)
            return
        accept = base64.b64encode(hashlib.sha1(
            (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101, "Switching Protocols")
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        self.wfile.write(ws_frame(f"<tv_state value='{STATE['tv']}'/>\n"))
        WS_CLIENTS.append(self.connection)
        try:
            while True:                      # keep it open; we only push
                if not self.connection.recv(4096):
                    break
        except Exception:
            pass
        finally:
            if self.connection in WS_CLIENTS:
                WS_CLIENTS.remove(self.connection)

    def log_message(self, *a):
        pass


# ---------------------------------------------------------------- CDP (browser control)
def cdp_call(method, **params):
    ws = CDP["ws"]
    if ws is None:
        return None
    CDP["seq"] += 1
    mid = CDP["seq"]
    with LOCK:
        ws.send(json.dumps({"id": mid, "method": method, "params": params}))
        deadline = time.time() + 15
        while time.time() < deadline:
            msg = json.loads(ws.recv())
            if msg.get("id") == mid:
                return msg.get("result")
    return None


def cdp_connect(port, timeout=25):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/json") as r:
                targets = json.load(r)
            page = next((t for t in targets if t.get("type") == "page"), None)
            if page:
                import websocket
                ws = websocket.create_connection(page["webSocketDebuggerUrl"], timeout=10)
                CDP["ws"] = ws
                cdp_call("Page.enable")
                cdp_call("Runtime.enable")
                cdp_call("Emulation.setDeviceMetricsOverride",
                         width=1280, height=720, deviceScaleFactor=1.5, mobile=False)
                log("cdp: attached to", page.get("url", "?"))
                return True
        except Exception:
            pass
        time.sleep(0.5)
    return False


def send_key(code):
    km = KEYS.get(code)
    if not km:
        log(f"key {code}: not mapped")
        return
    name, vk = km
    STATE["last_key"] = code
    if code in TV_CONTROL:
        # the set consumes these and the page is dismissed; MENU would open the portal
        log(f"key {code} ({name}): TV control -> page closed")
        cdp_call("Page.navigate", url="about:blank")
        push_state("PORTAL" if code == 1048 else "SOURCE")
        return
    log(f"key {code} ({name}): dispatched to the page")
    for typ in ("rawKeyDown", "keyUp"):
        cdp_call("Input.dispatchKeyEvent", type=typ, key=name,
                 windowsVirtualKeyCode=vk, nativeVirtualKeyCode=vk,
                 code=name if len(name) > 1 else "Key" + name.upper())


def load(url):
    log("browserseturl:", url)
    res = cdp_call("Page.navigate", url=url)
    push_state("PORTAL")


def shot(path):
    res = cdp_call("Page.captureScreenshot", format="png")
    if not res:
        return False
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(base64.b64decode(res["data"]))
    log("screenshot:", path)
    return True


# ---------------------------------------------------------------- HTTP: page + CGI + DIAL + sim API
def cgi_act(qs):
    a = urllib.parse.parse_qs(qs).get("a", [""])[0]
    if a == "alive":
        STATE["alive"] = int(time.time())
        return "alive"
    if a == "boot-ok":
        STATE["alive"] = int(time.time())
        STATE["boot_ok"] = True
        log("boot handshake: page rendered")
        return "boot recorded"
    if a == "tv":
        log("tile: TV -> EXIT (stock UI)")
        cdp_call("Page.navigate", url="about:blank")
        push_state("SOURCE")
        return "exit sent"
    if a == "sources":
        log("tile: Sources -> source list")
        cdp_call("Page.navigate", url="about:blank")
        push_state("SOURCE")
        return "sources"
    if a == "apps":
        log("tile: Apps -> stock portal")
        cdp_call("Page.navigate", url="about:blank")
        push_state("PORTAL")
        return "apps"
    if a == "sleep":
        log("tile/action: suspend (power key)")
        push_state("STANDBY")
        return "sleep"
    if a == "standby":
        log("command: Standby (ignored on the real set)")
        return "standby"
    if a == "key":
        code = int(urllib.parse.parse_qs(qs).get("c", ["0"])[0])
        send_key(code)
        return f"key {code}"
    return "usage"


def cgi_stats():
    cpu = 0
    ram = 0
    try:
        with open("/proc/stat") as f:
            p = [int(x) for x in f.readline().split()[1:]]
        idle, total = p[3] + p[4], sum(p)
        cpu = max(0, min(100, round(100 * (total - idle) / total)))
        with open("/proc/meminfo") as f:
            info = {l.split(":")[0]: int(l.split()[1]) for l in f}
        ram = round(100 * (info["MemTotal"] - info.get("MemAvailable", 0)) / info["MemTotal"])
    except Exception:
        pass
    return json.dumps({"host": "tvsim", "kernel": "4.9.118-sim", "cpu": cpu, "ram": ram,
                       "deploy": "up 0s", "temp": 42, "uptime": 0, "ts": int(time.time())})


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _send(self, code, body, ctype="text/plain"):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _static(self, path):
        path = path.split("?")[0]
        rel = path.lstrip("/")
        if rel in ("", "omarchy", "omarchy/"):
            rel = "omarchy/index.html"
        full = os.path.normpath(os.path.join(ROOT, rel))
        if not full.startswith(ROOT) or not os.path.isfile(full):
            self._send(404, "not found")
            return
        ctype = {"html": "text/html", "js": "application/javascript", "css": "text/css",
                 "json": "application/json", "png": "image/png", "webp": "image/webp",
                 "ttf": "font/ttf", "mp4": "video/mp4"}.get(full.rsplit(".", 1)[-1], "application/octet-stream")
        with open(full, "rb") as f:
            self._send(200, f.read(), ctype)

    def do_GET(self):
        p = urllib.parse.urlparse(self.path)
        if p.path == "/dd.xml":
            self._send(200, '<?xml version="1.0"?><root><device><deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>'
                            '<friendlyName>TVSIM</friendlyName><modelName>Vestel_MB230</modelName>'
                            '<UDN>uuid:00000000-0000-0000-0000-000000000000</UDN></device></root>', "text/xml")
            return
        if p.path == "/cgi-bin/act":
            self._send(200, cgi_act(p.query), "text/plain")
            return
        if p.path == "/cgi-bin/stats":
            self._send(200, cgi_stats(), "application/json")
            return
        if p.path == "/cgi-bin/net.js":
            self._send(200, 'window.OMARCHY_NET_NAME = "eth0 · 10.0.0.2 up (sim)";\n', "application/javascript")
            return
        if p.path.startswith("/sim/"):
            q = urllib.parse.parse_qs(p.query)
            if p.path == "/sim/key":
                send_key(int(q.get("code", ["0"])[0]))
                self._send(200, "ok")
            elif p.path == "/sim/load":
                load(q.get("url", [""])[0])
                self._send(200, "ok")
            elif p.path == "/sim/shot":
                ok = shot(q.get("path", ["/tmp/opencode/tvsim.png"])[0])
                self._send(200 if ok else 500, "ok" if ok else "no cdp")
            elif p.path == "/sim/state":
                self._send(200, json.dumps(STATE), "application/json")
            else:
                self._send(404, "?")
            return
        self._static(p.path)

    def do_POST(self):
        p = urllib.parse.urlparse(self.path)
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n).decode(errors="replace") if n else ""
        if p.path.startswith("/apps/"):
            m = body.replace("\n", " ")
            if "<key" in m:
                import re
                code = re.search(r"code=['\"]?(\d+)", m)
                if code:
                    send_key(int(code.group(1)))
            elif "browserseturl" in m:
                import re
                url = re.search(r"url=['\"]([^'\"]+)", m)
                if url:
                    load(url.group(1))
            elif "<command>" in m:
                import re
                cmd = re.search(r"<command>([^<]*)</command>", m)
                log("command:", cmd.group(1) if cmd else "?")
            else:
                log("POST /apps:", m[:80])
            self._send(201, "created")
            return
        self._send(404, "?")

    def log_message(self, fmt, *args):
        pass


# ---------------------------------------------------------------- main
def serve(port, ws_port, dial_port):
    for p in (port, ws_port, dial_port, dial_port + 1):
        srv = ThreadingHTTPServer(("127.0.0.1", p), StateWS if p == ws_port else Handler)
        srv.daemon_threads = True
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        log(f"listening on 127.0.0.1:{p}" + ("  (ws)" if p == ws_port else ""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8181)
    ap.add_argument("--ws-port", type=int, default=7681)
    ap.add_argument("--dial-port", type=int, default=56789)
    ap.add_argument("--cdp-port", type=int, default=9222)
    ap.add_argument("--no-browser", action="store_true")
    ap.add_argument("--headful", action="store_true")
    ap.add_argument("--url", default="http://127.0.0.1:8181/omarchy/")
    args = ap.parse_args()

    serve(args.port, args.ws_port, args.dial_port)

    if not args.no_browser:
        chrome = shutil.which("chromium") or shutil.which("chromium-browser") or shutil.which("google-chrome")
        if not chrome:
            log("no chromium found - running without a browser")
        else:
            profile = "/tmp/opencode/tvsim-profile"
            cmd = [chrome, f"--remote-debugging-port={args.cdp_port}",
                   "--user-data-dir=" + profile, "--no-first-run", "--no-default-browser-check",
                   "--disable-gpu", "--window-size=1280,720"]
            if not args.headful:
                cmd.append("--headless=new")
            cmd.append("about:blank")
            log("starting browser:", " ".join(cmd[:2]))
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if cdp_connect(args.cdp_port):
                load(args.url)
            else:
                log("cdp: could not attach (is the port free?)")

    log("ready - control it with tools/tvsimctl.sh (key/load/shot/state), Ctrl-C to stop")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        log("bye")


if __name__ == "__main__":
    main()
