#!/bin/sh
# Offline differential: execute the shipped Aurora UserPreferences /
# NovoPictureSettings picture accessors on the shipped settingsDB blob under
# qemu-arm-static and compare every field with common/picture_config.c.
# No TV, /dev/ntkdvb, vendor process or fbdev.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
SYSROOT=$("$ARM" -print-sysroot)
QEMU=${OA_QEMU:-qemu-arm-static}
ELF=${OA_AURORA_ELF:-$ROOT/artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf}
DB=${OA_SETTINGSDB:-$ROOT/artifacts/dumps/apdat_files/sqlite_db/settingsDB.db}
STAGE=${OA_PICTURE_OBJECT_STAGE:-/tmp/oa-picture-object}

for f in "$ELF" "$DB"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done
command -v "$QEMU" >/dev/null || { echo "missing $QEMU" >&2; exit 2; }

rm -rf "$STAGE"
mkdir -p "$STAGE"

last_zoom=$(python3 - "$DB" "$STAGE/blob.bin" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
blob = db.execute(
    "select m_picture_setting_configuration from Settings").fetchone()[0]
if len(blob) != 20 * 0x240:
    raise SystemExit(f"blob is {len(blob)} bytes, expected {20 * 0x240}")
open(sys.argv[2], "wb").write(blob)
print(db.execute("select m_last_zoom_mode from Settings").fetchone()[0])
PY
)

"$ARM" -I"$ROOT/openaurora/include" -O2 -g -Wall -Wextra -Werror -pedantic \
    -no-pie -Wl,-Ttext-segment=0x20000000 \
    -o "$STAGE/picture_object_vendor" \
    "$ROOT/openaurora/tests/picture_object_vendor.c" \
    "$ROOT/openaurora/common/picture_config.c" -pthread

"$QEMU" -L "$SYSROOT" "$STAGE/picture_object_vendor" "$ELF" \
    "$STAGE/blob.bin" "$last_zoom"
