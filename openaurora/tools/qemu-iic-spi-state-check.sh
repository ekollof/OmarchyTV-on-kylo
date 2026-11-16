#!/bin/sh
# Offline stateful IIC/SPI check: run the real libNDAL3 Hal_Iic_* and the
# recovered SPI transport under qemu against the opt-in TV I/O model. No TV,
# kernel driver, I2C or SPI hardware access.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
QEMU=${OA_QEMU:-qemu-arm-static}
ROOTFS=${OA_QEMU_ROOTFS:-$ROOT/artifacts/workbench/rootfs/root}
VENDOR=$ROOT/artifacts/workbench/rootfs/vendor
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-iic-spi}

for f in "$ROOT/openaurora/tests/fake_tv_io.c" \
         "$ROOT/openaurora/tests/fake_iic_spi_probe.c" \
         "$ROOT/openaurora/common/spi_dev.c" \
         "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done
command -v "$QEMU" >/dev/null || { echo "missing $QEMU" >&2; exit 2; }

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$VENDOR/lib/libNDAL3.so" "$VENDOR/lib/libNDAL_EX.so" \
   "$VENDOR/lib/libv4l2.so.0" "$VENDOR/lib/libv4lconvert.so.0" \
   "$VENDOR/lib/libnteec.so" "$VENDOR/lib/libmhash.so.2" "$STAGE/"
cp "$VENDOR/lib/libs/libasound.so.2" "$STAGE/"
"$ARM" -O2 -fPIC -shared -std=c11 -Wall -Wextra -Werror -pedantic \
    -o "$STAGE/fake_tv_io.so" \
    "$ROOT/openaurora/tests/fake_tv_io.c" -ldl
"$ARM" -O2 -D_GNU_SOURCE -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT/openaurora/include" \
    -o "$STAGE/probe" \
    "$ROOT/openaurora/tests/fake_iic_spi_probe.c" \
    "$ROOT/openaurora/common/spi_dev.c" \
    -L"$STAGE" -l:libNDAL3.so -l:libNDAL_EX.so -l:libasound.so.2 \
    -Wl,--allow-shlib-undefined -Wl,-rpath,'$ORIGIN'

rm -f "$STAGE/io.log" "$STAGE/state" "$STAGE/out"
LD_LIBRARY_PATH="$STAGE" LD_PRELOAD="$STAGE/fake_tv_io.so" \
OA_TV_IO_LOG="$STAGE/io.log" OA_TV_IO_STATE="$STAGE/state" \
    "$QEMU" -L "$ROOTFS" "$STAGE/probe" >"$STAGE/out" 2>&1
STATUS=$?

python3 - "$STAGE" "$STATUS" <<'PY'
import os
import sys

stage, status = sys.argv[1], int(sys.argv[2])
state = open(os.path.join(stage, "state")).read() if os.path.exists(os.path.join(stage, "state")) else ""
log = open(os.path.join(stage, "io.log")).read() if os.path.exists(os.path.join(stage, "io.log")) else ""
out = open(os.path.join(stage, "out")).read()
assert status == 0, out
assert "addr=0x34 len=4 flags=R byte0=0xa5" in state, state + "\n" + out
assert "spi offset=16 size=4 byte0=0x10 byte1=0x11" in state, state + "\n" + out
assert "iic addr=0x34 len=4 byte0=a5 byte1=a6" in out, out
assert "spi open=0 read=1 byte0=10 byte1=11" in out, out
assert "flags=W" in log and "flags=R" in log, log + "\n" + out
assert "spi offset=16 size=4" in log, log + "\n" + out
print("ok qemu stateful IIC/SPI slice: real Hal_Iic_Read/Write and recovered "
      "SPI transport decoded into pin-independent bus state")
PY
