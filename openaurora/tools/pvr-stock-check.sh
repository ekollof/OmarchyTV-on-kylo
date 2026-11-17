#!/bin/sh
# Offline stock PVR-list candidate: run the gated ui.pvr_stock screen with the
# vendor fonts and the recovered row icons, and write the real-font recorder
# image. There is no stock PVR capture, so this is candidate evidence only; it
# is not a pixel-parity or scanout check.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/openaurora/build}
MODULES=$BUILD/tests/ui-pvr-stock/modules
FONT_INDEX=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.txt
FONT_PACK=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.bin
OUT=$ROOT/artifacts/analysis/pvr-stock-2026-11-17/pvr-stock

for f in "$FONT_INDEX" "$FONT_PACK"; do
    [ -f "$f" ] || { echo "skip PVR stock capture: missing $f"; exit 0; }
done

make -C "$ROOT/openaurora" -j"${JOBS:-4}" \
    "$BUILD/tests/ui_pvr_stock" "$MODULES" >/dev/null

rm -rf "$OUT.ui"
mkdir -p "$OUT.ui"
python3 - "$ROOT/artifacts/ui-assets" "$OUT.ui" <<'PY'
import struct, sys
from pathlib import Path
from PIL import Image
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
# PlayListDialog::createComponents row indicators (image ids 54/278/30/277/
# 276/99 and 6/8) plus nothing else. Only icon_record_small is painted today.
for name in ("icon_record_small", "icon_right_arrow", "icon_lock",
             "icon_play_disallowed", "icon_exclamation_mark",
             "icon_lost_signal", "icon_tv_small", "icon_radio_small"):
    p = src / f"{name}.png"
    if not p.exists():
        continue
    im = Image.open(p).convert("RGBA")
    with open(dst / f"{name}.rgba", "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())
PY

OA_PVR_FONT_INDEX="$FONT_INDEX" OA_PVR_FONT_PACK="$FONT_PACK" \
OA_PVR_UI_DIR="$OUT.ui" OA_PVR_STOCK_CAPTURE="$OUT.raw" \
OA_PVR_SYNOPSIS_CAPTURE="$OUT-synopsis.raw" \
    "$BUILD/tests/ui_pvr_stock" "$MODULES"

python3 "$ROOT/openaurora/tools/rawrgba-to-png.py" "$OUT.raw" 1920 1080 "$OUT.png"
python3 "$ROOT/openaurora/tools/rawrgba-to-png.py" \
    "$OUT-synopsis.raw" 1920 1080 "$OUT-synopsis.png"
rm -f "$OUT.raw" "$OUT-synopsis.raw"
rm -rf "$OUT.ui"
echo "wrote $OUT.png and $OUT-synopsis.png (recorder candidates; no stock PVR reference)"
