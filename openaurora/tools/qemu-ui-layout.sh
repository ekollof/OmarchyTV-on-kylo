#!/bin/sh
# Run the real ARM UI stack under qemu-arm-static with the offline framebuffer
# module, capture frame(s), and stop it. No TV or fbdev access.
#
#   tools/qemu-ui-layout.sh STAGE_DIR
#
# STAGE_DIR must contain: modules/ (mod_sim_display.so, mod_ui_engine.so,
# mod_ui_menu.so, mod_ui_vendor.so), config, menus/, ui/ and an optional
# frame prefix. The config selects ui.screen, ui.dir and ui_menu.file.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
STAGE=${1:?usage: qemu-ui-layout.sh STAGE_DIR}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
QEMU=${OA_QEMU:-qemu-arm-static}
HOST=$ROOT/openaurora/build-tv-v3/openaurora
SOCK=$STAGE/control
FRAME=$STAGE/frame
LOG=$STAGE/log
MODULES=${OA_QEMU_MODULES:-"sim_display ui_engine ui_menu ui_vendor"}
KEY=${OA_QEMU_KEY:-}
KEYS=${OA_QEMU_KEYS:-}
ACTION=${OA_QEMU_ACTION:-}
HOLD=${OA_QEMU_HOLD_MS:-0}

test -x "$HOST" || { echo "missing $HOST" >&2; exit 2; }
test -d "$STAGE/modules" || { echo "missing $STAGE/modules" >&2; exit 2; }
test -f "$STAGE/config" || { echo "missing $STAGE/config" >&2; exit 2; }

rm -f "$SOCK" "$FRAME".* "$LOG"
OA_NAV_CAPTURE=$FRAME "$QEMU" -L "$ROOTFS" "$HOST" \
    --modules "$STAGE/modules" --config "$STAGE/config" \
    --sock "$SOCK" --log 2 >"$LOG" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true' EXIT INT TERM

python3 - "$SOCK" "$MODULES" "$FRAME" "$KEY" "$KEYS" "$ACTION" "$HOLD" <<'PY'
import os
import socket
import sys
import time

sock = sys.argv[1]
modules = sys.argv[2].split()
frame = sys.argv[3]
key = sys.argv[4]
keys = sys.argv[5].split(",") if sys.argv[5] else []
action = sys.argv[6]
hold = int(sys.argv[7] or "0")
for _ in range(100):
    if os.path.exists(sock):
        break
    time.sleep(0.05)
else:
    sys.exit("control socket did not appear")
s = socket.socket(socket.AF_UNIX)
s.settimeout(10)
s.connect(sock)

def command(text):
    s.sendall((text + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
    return data.decode().strip()

for module in modules:
    for verb in ("load", "start"):
        reply = command(f"{verb} {module}")
        if not reply.startswith("ok"):
            sys.exit(f"{verb} {module}: {reply}")
for _ in range(100):
    if os.path.exists(frame + ".0"):
        break
    time.sleep(0.05)
if action:
    reply = command("ui-action " + action)
    if not reply.startswith("ok"):
        sys.exit(f"ui-action {action}: {reply}")
if key:
    keys.append(key)
for one in keys:
    reply = command("key " + one)
    if not reply.startswith("ok"):
        sys.exit(f"key {one}: {reply}")
if (action or keys) and hold:
    time.sleep(hold / 1000.0)
s.close()
PY

for _ in $(seq 1 100); do
    test -f "$FRAME.0" && break
    sleep 0.05
done
test -f "$FRAME.0" || { echo "no captured frame" >&2; exit 3; }
echo "captured $FRAME.0"
