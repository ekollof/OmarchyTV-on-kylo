#!/bin/sh
# Offline real-fbdev check: run the ARM mod_display (real fb1 path) under
# qemu-arm-static with an LD_PRELOAD framebuffer that models the recovered
# fb1/OSD region/MAU contract. No TV, /dev/fb1 or kernel driver access.
#
#   tools/qemu-fb-real-check.sh
#
# Proves: open/var/fix/mmap, snapshot save, region write, pan, self-test present
# and snapshot restore through the real display module. The framebuffer model
# is a test boundary, not a kernel or scanout emulator.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
BUILD=$ROOT/openaurora/build-tv-v3
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-fb-real}

for f in "$ROOT/openaurora/tests/fake_fb_preload.c" \
         "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
         "$VENDOR/lib/libs/libasound.so.2" "$VENDOR/lib/libv4l2.so.0" \
         "$VENDOR/lib/libv4lconvert.so.0" "$VENDOR/lib/libnteec.so" \
         "$VENDOR/lib/libmhash.so.2"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done
command -v "$QEMU" >/dev/null || { echo "missing $QEMU" >&2; exit 2; }

make -C "$ROOT/openaurora" -j"${JOBS:-4}" TARGET=tv BUILD=build-tv-v3 CC="$ARM" \
    build-tv-v3/openaurora build-tv-v3/modules/mod_display.so \
    build-tv-v3/tests/mod_plat_stub.so >/dev/null

rm -rf "$STAGE"
mkdir -p "$STAGE/modules"
cp "$BUILD/openaurora" "$STAGE/openaurora"
cp "$BUILD/modules/mod_display.so" "$STAGE/modules/"
cp "$BUILD/tests/mod_plat_stub.so" "$STAGE/modules/mod_plat.so"
cp "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
   "$VENDOR/lib/libv4l2.so.0" "$VENDOR/lib/libv4lconvert.so.0" \
   "$VENDOR/lib/libnteec.so" "$VENDOR/lib/libmhash.so.2" "$STAGE/"
cp "$VENDOR/lib/libs/libasound.so.2" "$STAGE/"
"$ARM" -O2 -fPIC -shared -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT/openaurora/include" \
    -o "$STAGE/fake_fb_preload.so" \
    "$ROOT/openaurora/tests/fake_fb_preload.c" -ldl

cat >"$STAGE/config" <<EOF
autoload=
plat.enable=1
display.fb=/dev/fb1
display.restore_snapshot=1
display.set_region=1
display.pan=1
display.mirror=0
display.test=2
display.cleanup_delay_ms=0
EOF

rm -f "$STAGE/control" "$STAGE/changed" "$STAGE/final" "$STAGE/fb.log"
LD_LIBRARY_PATH="$STAGE" LD_PRELOAD="$STAGE/fake_fb_preload.so" \
OA_FAKE_FB_CHANGED="$STAGE/changed" OA_FAKE_FB_STATE="$STAGE/final" \
FAKE_FB_LOG="$STAGE/fb.log" \
    "$QEMU" -L "$ROOTFS" "$STAGE/openaurora" \
    --modules "$STAGE/modules" --config "$STAGE/config" \
    --sock "$STAGE/control" --log 3 >"$STAGE/out" 2>&1 &
PID=$!
cleanup() {
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

python3 - "$STAGE" <<'PY'
import os
import socket
import sys
import time

stage = sys.argv[1]
sock = os.path.join(stage, "control")
changed = os.path.join(stage, "changed")
final = os.path.join(stage, "final")

def wait_until(pred, ms, what):
    end = time.time() + ms / 1000.0
    while time.time() < end:
        if pred():
            return
        time.sleep(0.02)
    raise SystemExit(f"timeout waiting for {what}")

def command(text):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(5)
    s.connect(sock)
    s.sendall((text + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
    s.close()
    return data.decode().strip()

def retry(text, tries=30):
    last = ""
    for _ in range(tries):
        last = command(text)
        if last == "ok":
            return last
        time.sleep(0.1)
    return last

def state(path):
    text = open(path).read().strip()
    out = {}
    for part in text.split():
        at = part.find("=")
        if at > 0:
            out[part[:at]] = part[at + 1:]
    return out

wait_until(lambda: os.path.exists(sock), 5000, "control socket")
assert command("load plat") == "ok plat"
assert command("start plat") == "ok"
assert command("load display") == "ok display"
assert command("start display") == "ok"
assert command("display-info") == "display 1920x1080 32bpp 2 frames"
wait_until(lambda: os.path.exists(changed), 5000, "self-test present")
s = state(changed)
assert s["current_pixels"] != s["initial_pixels"], s
assert s["current_region"] != s["initial_region"], s
assert s["current_yoffset"] != s["initial_yoffset"], s
assert retry("stop display") == "ok"
wait_until(lambda: os.path.exists(final), 5000, "snapshot restore state")
f = state(final)
assert f["current_pixels"] == f["initial_pixels"], f
assert f["current_region"] == f["initial_region"], f
assert f["current_yoffset"] == f["initial_yoffset"], f
assert f["current_mau"] == f["initial_mau"], f
assert command("quit") == "bye"
print("ok qemu real fbdev: mod_display open/var/fix/mmap, self-test present, "
      "region/pan change and full snapshot restore")
PY
wait "$PID" 2>/dev/null || true
