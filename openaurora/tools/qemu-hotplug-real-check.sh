#!/bin/sh
# Offline real hotplug slice: run the real ARM mod_plat + mod_source with the
# shipped NDAL HAL and the recovered DEV GPIO descriptor ioctl under
# qemu-arm-static. No TV, /dev/ntgpio, GPIO, PWM or STBC access.
#
# The fake TV io boundary answers DEV_GPIO_GetDescription with the live TV
# mapping (HDMI1..4 -> 274..277, active low; MHL_CDSENSE/HDMI5 absent) and
# logs the NDAL STBC ext-control payload. The test proves the real provider
# descriptor parse, the MHL/HDMI5 absence path and the STBC (pin >= 200)
# routing. It is not a hardware effect or scanout claim.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
BUILD=$ROOT/openaurora/build-tv-v3
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-hotplug-real}

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
    build-tv-v3/modules/mod_source.so >/dev/null

rm -rf "$STAGE"
mkdir -p "$STAGE/modules"
cp "$BUILD/openaurora" "$STAGE/openaurora"
cp "$BUILD/modules/mod_plat.so" "$BUILD/modules/mod_source.so" "$STAGE/modules/"
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
source.enable=1
source.hotplug_pulse=1
source.cleanup_delay_ms=0
EOF

rm -f "$STAGE/control" "$STAGE/io.log" "$STAGE/out"
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
out = os.path.join(stage, "out")
log = os.path.join(stage, "io.log")

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
    assert command("load source") == "ok source", open(out).read()[-4000:]
    assert command("start source") == "ok", open(out).read()[-4000:]
    wait_until(lambda: "hotplug pulse finished" in open(out).read(),
               8000, "hotplug pulse completion")
    text = open(out).read()
    assert "HDMI hotplug pulse started" in text, text[-4000:]
    assert "HDMI hotplug pulse finished (4 descriptor(s))" in text, text[-4000:]

    wait_until(lambda: os.path.exists(log), 3000, "TV io log")
    io = open(log).read()
    for needle in (
        "open /dev/ntgpio",
        "gpio_desc name=HDMI_HOTPLUG_1 pin=274 active=low",
        "gpio_desc name=HDMI_HOTPLUG_2 pin=275 active=low",
        "gpio_desc name=HDMI_HOTPLUG_3 pin=276 active=low",
        "gpio_desc name=HDMI_HOTPLUG_4 pin=277 active=low",
        "gpio_desc_missing name=MHL_CDSENSE errno=524",
        "gpio_desc_missing name=HDMI_HOTPLUG_5 errno=524",
    ):
        assert needle in io, f"missing {needle}\n{io}"
    # Pins 274..277 are >= 200, so the pulse must use the STBC path, never
    # Hal_Gpio_Write on those pins. DEV_GPIO_SetLevel maps
    # (pin+0x38)&0xff and 1 -> 2 (inactive), 0 -> 3 (active).
    assert "gpio_value" not in io, io
    pairs = []
    channel = None
    for line in io.splitlines():
        if line.startswith("ext_ctl") and "idx=1" in line:
            channel = int(line.rsplit("byte0=", 1)[1])
        elif line.startswith("ext_ctl") and "idx=2" in line:
            value = int(line.rsplit("byte0=", 1)[1])
            pairs.append((channel, value))
    assert pairs == [(74, 2), (75, 2), (76, 2), (77, 2),
                     (74, 3), (75, 3), (76, 3), (77, 3)], pairs
    assert command("stop source") == "ok", open(out).read()[-4000:]
    assert command("stop plat") == "ok", open(out).read()[-4000:]
    assert command("quit") == "bye"
    print("ok qemu real hotplug: descriptor ioctl returned 274..277 active-low, "
          "MHL/HDMI5 absent, four-pin pulse used the STBC ext-control path")
finally:
    pass
PY
wait "$PID" 2>/dev/null || true
