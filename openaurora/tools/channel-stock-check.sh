#!/bin/sh
# Offline stock channel-list capture check: render the gated stock mode with
# the vendor fonts and the p0/04 analog channel rows, then compare the panel
# regions with the BGRA-corrected stock capture. Recorder/CPU only, not scanout.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/openaurora/build}
MODULES=$BUILD/tests/ui-channels/modules
FONT_INDEX=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.txt
FONT_PACK=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data.bin
REF=$ROOT/artifacts/analysis/channel-list-geometry-2026-11-17/p0-04-channel-list-corrected.png
REGIONS=$ROOT/artifacts/analysis/channel-list-geometry-2026-11-17/regions.json
OUT=$BUILD/tests/channel-stock

for f in "$FONT_INDEX" "$FONT_PACK" "$REF" "$REGIONS"; do
    [ -f "$f" ] || { echo "skip channel stock capture: missing $f"; exit 0; }
done

make -C "$ROOT/openaurora" -j"${JOBS:-4}" "$BUILD/tests/ui_channels_stock" \
    "$BUILD/tests/ui-channels/modules" >/dev/null

rm -rf "$OUT.ui"
mkdir -p "$OUT.ui"
python3 - "$ROOT/artifacts/ui-assets/channel_small.png" "$OUT.ui/channel_small.rgba" <<'PY'
import struct, sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGBA')
with open(sys.argv[2], 'wb') as f:
    f.write(b'OAI1')
    f.write(struct.pack('<II', im.width, im.height))
    f.write(im.tobytes())
PY

OA_CHANNELS_FONT_INDEX="$FONT_INDEX" OA_CHANNELS_FONT_PACK="$FONT_PACK" \
OA_CHANNELS_UI_DIR="$OUT.ui" OA_CHANNELS_STOCK_CAPTURE="$OUT.raw" \
    "$BUILD/tests/ui_channels_stock" "$MODULES"

python3 "$ROOT/openaurora/tools/rawrgba-to-png.py" "$OUT.raw" 1920 1080 "$OUT.png"
python3 "$ROOT/openaurora/tools/capture_compare.py" --reference "$REF" \
    --candidate "$OUT.png" --manifest "$REGIONS" | tee "$OUT.compare.txt"

python3 - "$OUT.compare.txt" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
limits = {'panel': 3.5, 'focus_row': 2.0, 'row2': 3.5}
found = {}
for line in text.splitlines():
    m = re.match(r'region (\w+): .*MAE8=([0-9.]+)', line)
    if m:
        found[m.group(1)] = float(m.group(2))
for name, limit in limits.items():
    if name not in found:
        raise SystemExit(f'missing region {name}')
    if found[name] > limit:
        raise SystemExit(f'{name} MAE8 {found[name]} > {limit}')
print('ok channel stock capture:', ', '.join(f'{k} MAE8={found[k]}' for k in limits))
PY
