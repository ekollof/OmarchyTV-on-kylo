#!/bin/sh
# Offline real panel/GPIO/PWM slice: run the ARM mod_plat + mod_panel with the
# shipped NDAL HAL under qemu-arm-static and a permissive, logged device
# boundary. No TV, kernel driver, GPIO, PWM or panel access.
#
# Proves the recovered HAL call path and ordering:
#   Hal_Gpio_Init -> /dev/ntgpio
#   Hal_Pwm_Open/Set -> 0x403c4c4c / 0xc03c4c53
#   Hal_Gpio_Write -> 0x670a / 0x670e / 0x670c
# It is a simulation, not hardware-effect or scanout acceptance.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
BUILD=$ROOT/openaurora/build-tv-v3
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-panel-real}

for f in "$ROOT/openaurora/tests/fake_tv_io.c" \
         "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
         "$VENDOR/lib/libs/libasound.so.2" "$VENDOR/lib/libv4l2.so.0" \
         "$VENDOR/lib/libv4lconvert.so.0" "$VENDOR/lib/libnteec.so" \
         "$VENDOR/lib/libmhash.so.2"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done
command -v "$QEMU" >/dev/null || { echo "missing $QEMU" >&2; exit 2; }

make -C "$ROOT/openaurora" -j"${JOBS:-4}" TARGET=tv BUILD=build-tv-v3 CC="$ARM" \
    build-tv-v3/openaurora build-tv-v3/modules/mod_plat.so \
    build-tv-v3/modules/mod_panel.so >/dev/null

rm -rf "$STAGE"
mkdir -p "$STAGE/modules"
cp "$BUILD/openaurora" "$STAGE/openaurora"
cp "$BUILD/modules/mod_plat.so" "$BUILD/modules/mod_panel.so" "$STAGE/modules/"
cp "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
   "$VENDOR/lib/libv4l2.so.0" "$VENDOR/lib/libv4lconvert.so.0" \
   "$VENDOR/lib/libnteec.so" "$VENDOR/lib/libmhash.so.2" "$STAGE/"
cp "$VENDOR/lib/libs/libasound.so.2" "$STAGE/"
"$ARM" -O2 -fPIC -shared -std=c11 -Wall -Wextra -Werror -pedantic \
    -o "$STAGE/fake_tv_io.so" \
    "$ROOT/openaurora/tests/fake_tv_io.c" -ldl

cat >"$STAGE/config" <<EOF
autoload=
plat.enable=1
plat.driver_wait=0
plat.drm_init=0
plat.wdt_probe=0
panel.standby=1
panel.power_on_start=1
panel.start=100
panel.channel=0
panel.freq=30000
panel.mode=0
panel.curve=1
panel.duty_max=4095
panel.cleanup_delay_ms=0
EOF

rm -f "$STAGE/control" "$STAGE/io.log"
LD_LIBRARY_PATH="$STAGE" LD_PRELOAD="$STAGE/fake_tv_io.so" \
OA_TV_IO_LOG="$STAGE/io.log" \
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
log = os.path.join(stage, "io.log")
out = os.path.join(stage, "out")

def wait_until(pred, ms, what):
    end = time.time() + ms / 1000.0
    while time.time() < end:
        if pred():
            return
        time.sleep(0.02)
    raise SystemExit(f"timeout waiting for {what}\n" + open(out).read()[-4000:])

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

try:
    wait_until(lambda: os.path.exists(sock), 5000, "control socket")
    assert command("load plat") == "ok plat", open(out).read()[-4000:]
    assert command("start plat") == "ok", open(out).read()[-4000:]
    assert command("load panel") == "ok panel", open(out).read()[-4000:]
    assert command("start panel") == "ok", open(out).read()[-4000:]
    assert command("panel-state") == "panel percent 100", command("panel-state")
    wait_until(lambda: os.path.exists(log), 3000, "HAL I/O log")
    text = open(log).read()
    lines = text.splitlines()
    for needle in ("open /dev/ntgpio",
                   "pin=110 value=1", "pin=111 value=0"):
        assert needle in text, f"missing {needle}\n{text}"
    assert any("pwm_open" in l and "w0=0" in l and "w2=30000" in l and
               "w3=1000" in l for l in lines), text
    assert any("pwm_set" in l and "w0=0" in l and "w2=30000" in l and
               "w3=1000" in l for l in lines), text
    idx110 = text.index("pin=110 value=1")
    idxpwm = text.index("pwm_set")
    idx111 = text.index("pin=111 value=0")
    assert idx110 < idxpwm < idx111, "panel power/PWM/backlight order wrong\n" + text
    assert command("stop panel") == "ok"
    assert command("stop plat") == "ok"
    assert command("quit") == "bye"
    print("ok qemu real panel slice: NDAL opened the TV device boundary, "
          "PWM open/set and GPIO 110/111 write ioctls executed")
finally:
    pass
PY
wait "$PID" 2>/dev/null || true
