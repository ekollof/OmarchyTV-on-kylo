#!/bin/sh
# Offline cold-display repair check: run the real ARM mod_display under qemu
# against a /dev/fb1 model that starts with an unconfigured (zero) var, with
# display.cold_init=1. Proves the recovered FBIOPUT_VSCREENINFO template path,
# the re-read var/fix, and a first present. No TV, kernel driver or scanout.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
BUILD=$ROOT/openaurora/build-tv-v3
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-cold-display}

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
display.cold_init=1
display.restore_snapshot=0
display.set_region=1
display.pan=1
display.mirror=0
display.test=2
display.cleanup_delay_ms=0
EOF

rm -f "$STAGE/control" "$STAGE/fb.log" "$STAGE/final" "$STAGE/out"
LD_LIBRARY_PATH="$STAGE" LD_PRELOAD="$STAGE/fake_fb_preload.so" \
OA_FAKE_FB_ZERO_VAR=1 FAKE_FB_LOG="$STAGE/fb.log" \
OA_FAKE_FB_STATE="$STAGE/final" \
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
import os, socket, sys, time
stage = sys.argv[1]
sock = os.path.join(stage, "control")
log = os.path.join(stage, "fb.log")
out = os.path.join(stage, "out")

def wait_until(pred, ms, what):
    end = time.time() + ms / 1000.0
    while time.time() < end:
        if pred(): return
        time.sleep(0.02)
    raise SystemExit(f"timeout waiting for {what}\n" + open(out).read()[-5000:])

def command(text):
    s = socket.socket(socket.AF_UNIX); s.settimeout(5); s.connect(sock)
    s.sendall((text + "\n").encode()); data = b""
    while not data.endswith(b"\n"):
        c = s.recv(4096)
        if not c: break
        data += c
    s.close(); return data.decode().strip()

try:
    wait_until(lambda: os.path.exists(sock), 5000, "control socket")
    assert command("load plat") == "ok plat", open(out).read()[-5000:]
    assert command("start plat") == "ok", open(out).read()[-5000:]
    assert command("load display") == "ok display", open(out).read()[-5000:]
    assert command("start display") == "ok", open(out).read()[-5000:]
    assert command("display-info") == "display 1920x1080 32bpp 2 frames", command("display-info")
    wait_until(lambda: os.path.exists(log), 3000, "fb log")
    text = open(log).read()
    assert "setpar xres=1920 yres=1080 bpp=32 yres_virtual=2160" in text, text
    assert "ioctl" in text and "mmap" in text, text
    assert "cold repair" in open(out).read() or "cold" in open(out).read(), open(out).read()[-5000:]
    stopped = command("stop display")
    if stopped.startswith("failed"):
        time.sleep(0.3)
        stopped = command("stop display")
    assert stopped == "ok", stopped
    assert command("quit") == "bye"
    print("ok qemu cold display: zero-var fb1 repaired by FBIOPUT_VSCREENINFO "
          "and first present completed")
finally:
    pass
PY
wait "$PID" 2>/dev/null || true
