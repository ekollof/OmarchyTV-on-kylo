#!/bin/sh
# Offline stock EPG candidate: run the gated ui.epg_stock timeline with the
# vendor fonts and write the real-font recorder image. There is no stock EPG
# capture, so this is candidate evidence only; it is not parity or scanout.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/openaurora/build}
MODULES=$BUILD/tests/ui-epg-stock/modules
FONT_INDEX=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.txt
FONT_PACK=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.bin
OUT=$ROOT/artifacts/analysis/epg-stock-2026-11-17/epg-stock

for f in "$FONT_INDEX" "$FONT_PACK"; do
    [ -f "$f" ] || { echo "skip EPG stock capture: missing $f"; exit 0; }
done

make -C "$ROOT/openaurora" -j"${JOBS:-4}" \
    "$BUILD/tests/ui_epg_stock" "$MODULES" >/dev/null

OA_EPG_FONT_INDEX="$FONT_INDEX" OA_EPG_FONT_PACK="$FONT_PACK" \
OA_EPG_I18N_PROFILE="$ROOT/artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin" \
OA_EPG_I18N_LANG=dut \
OA_EPG_STOCK_CAPTURE="$OUT.raw" \
OA_EPG_SYNOPSIS_CAPTURE="$OUT-synopsis.raw" \
    "$BUILD/tests/ui_epg_stock" "$MODULES"

python3 "$ROOT/openaurora/tools/rawrgba-to-png.py" "$OUT.raw" 1920 1080 "$OUT.png"
python3 "$ROOT/openaurora/tools/rawrgba-to-png.py" \
    "$OUT-synopsis.raw" 1920 1080 "$OUT-synopsis.png"
rm -f "$OUT.raw" "$OUT-synopsis.raw"
echo "wrote $OUT.png and $OUT-synopsis.png (recorder candidates; no stock EPG reference)"
