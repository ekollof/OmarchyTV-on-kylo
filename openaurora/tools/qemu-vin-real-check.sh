#!/bin/sh
# Offline real-VIN check: run the ARM mod_source (linked to the shipped
# libNDAL3.so) under qemu-arm-static against a fake /dev/video20 device,
# drive it over the real control socket, and assert the Hal_Vin ioctl
# sequence plus the async reload/stop gate. No TV or /dev/ntkdvif access.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
MOD=$ROOT/openaurora/build-tv-v3/modules
TEST=$ROOT/openaurora/build-tv-v3/tests
HOST=$ROOT/openaurora/build-tv-v3/openaurora
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-vin-real}

for f in "$ROOT/openaurora/tests/fake_vin_device.c" \
         "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
         "$VENDOR/lib/libs/libasound.so.2" "$VENDOR/lib/libv4l2.so.0" \
         "$VENDOR/lib/libv4lconvert.so.0" "$VENDOR/lib/libnteec.so" \
         "$VENDOR/lib/libmhash.so.2"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done
command -v "$QEMU" >/dev/null || { echo "missing $QEMU" >&2; exit 2; }

make -C "$ROOT/openaurora" -j"${JOBS:-4}" TARGET=tv BUILD=build-tv-v3 CC="$ARM" \
    build-tv-v3/openaurora build-tv-v3/modules/mod_source.so \
    build-tv-v3/tests/mod_plat_stub.so >/dev/null

rm -rf "$STAGE"
mkdir -p "$STAGE/modules"
cp "$HOST" "$STAGE/openaurora"
cp "$MOD/mod_source.so" "$STAGE/modules/"
cp "$TEST/mod_plat_stub.so" "$STAGE/modules/mod_plat.so"
cp "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
   "$VENDOR/lib/libv4l2.so.0" "$VENDOR/lib/libv4lconvert.so.0" \
   "$VENDOR/lib/libnteec.so" "$VENDOR/lib/libmhash.so.2" "$STAGE/"
cp "$VENDOR/lib/libs/libasound.so.2" "$STAGE/"
"$ARM" -O2 -fPIC -shared -o "$STAGE/fake_vin_device.so" \
    "$ROOT/openaurora/tests/fake_vin_device.c" -ldl

cat >"$STAGE/config" <<EOF
autoload=
plat.enable=1
source.enable=1
source.cleanup_delay_ms=400
EOF

LD_LIBRARY_PATH="$STAGE" LD_PRELOAD="$STAGE/fake_vin_device.so" \
FAKE_VIN_LOG="$STAGE/vin.log" \
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

def bounded(fn, ms, what):
    end = time.time() + ms / 1000.0
    while True:
        try:
            return fn()
        except OSError:
            if time.time() >= end:
                raise SystemExit(f"timeout waiting for {what}")
            time.sleep(0.02)

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

def timed(text):
    started = time.time()
    reply = command(text)
    return reply, (time.time() - started) * 1000.0

bounded(lambda: command("help"), 5000, "control socket")
assert command("load plat") == "ok plat"
assert command("start plat") == "ok"
assert command("load source") == "ok source"
assert command("start source") == "ok"
assert command("source-state") == "source type -1 available 1"

assert command("source-select 17") == "ok"
assert command("source-state") == "source type 17 available 1"
assert command("source-select 19") == "ok"
assert command("source-state") == "source type 19 available 1"

first, elapsed = timed("reload source")
assert first.startswith("failed"), first
assert elapsed < 1500, f"first reload took {elapsed:.0f}ms"
assert command("source-state") == "failed errno 19"
time.sleep(0.7)
assert command("reload source") == "ok"
assert command("source-state") == "source type -1 available 1"
assert command("source-select 17") == "ok"

first_stop, elapsed = timed("stop source")
assert first_stop.startswith("failed"), first_stop
assert elapsed < 1500, f"first stop took {elapsed:.0f}ms"
assert command("source-state") == "failed errno 19"
time.sleep(0.7)
assert command("stop source") == "ok"
assert command("unload source") == "ok"
assert command("quit") == "bye"

log = open(os.path.join(stage, "vin.log")).read()
# Source type 17 uses the two-fd path (idx 3): /dev/video28 + /dev/video21.
# Type 19 uses the single fd /dev/video66. These paths come from the current
# source/HAL mapping; the fake log is authoritative for this fixture.
assert log.count("open path=/dev/video28") == 2, log
assert log.count("open path=/dev/video21") == 2, log
assert "open path=/dev/video66" in log, log
assert "id=3" in log, log
assert "id=66" in log, log
assert log.count("start fd=") >= 3, log
assert log.count("ext_ctrls fd=") >= 4, log
assert log.count("close fd=") >= 3, log
close_at = log.index("close fd=")
second_open = log.index("open path=/dev/video28",
                        log.index("open path=/dev/video28") + 1)
assert close_at < second_open, "worker close must precede the reopened device"
print("ok qemu real VIN: libNDAL3 opened /dev/video28 + /dev/video21 for type 17, "
      "/dev/video66 for type 19, selected ids 3/66, started streams, and "
      "closed before the reload reopen")
PY

wait "$PID"
trap - EXIT INT TERM
