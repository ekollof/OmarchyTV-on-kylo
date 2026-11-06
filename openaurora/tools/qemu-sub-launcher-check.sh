#!/bin/sh
# Offline QEMU layout oracle for the JSON-driven sub-launcher row.
#
# Builds the ARM UI stack, runs it under qemu-arm-static with the offline
# framebuffer module, and compares the p0/03 picture-shortcut row against the
# vendor settings_top capture. Also samples the 300 ms card scale at 0/0.5/1.
# No TV, fbdev, input or vendor process is touched.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-sub-launcher}
UI=$ROOT/artifacts/workbench/source-stage-20261101/ui
MOD=$ROOT/openaurora/build-tv-v3/modules
TEST=$ROOT/openaurora/build-tv-v3/tests
HOST=$ROOT/openaurora/build-tv-v3/openaurora

if [ "${OA_QEMU_SKIP_BUILD:-0}" != 1 ]; then
    make -C "$ROOT/openaurora" -j"${JOBS:-4}" TARGET=tv BUILD=build-tv-v3 CC="$ARM" \
        build-tv-v3/openaurora \
        build-tv-v3/modules/mod_ui_engine.so \
        build-tv-v3/modules/mod_ui_menu.so \
        build-tv-v3/modules/mod_ui_vendor.so \
        build-tv-v3/tests/mod_sim_display.so >/dev/null
fi

for f in "$HOST" "$MOD/mod_ui_engine.so" "$MOD/mod_ui_menu.so" \
         "$MOD/mod_ui_vendor.so" "$TEST/mod_sim_display.so" \
         "$UI/settings_top.rgba" "$UI/settings_cat.rgba" \
         "$UI/settings_rail.rgba" "$ROOT/openaurora/menus/sub_launcher.json"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done

# The card cutouts are parity assets made from the vendor capture.
if [ ! -f "$UI/sublauncher_card_0.rgba" ]; then
    python3 "$ROOT/openaurora/tools/mk-sublauncher-cards.py" \
        "$UI/settings_top.rgba" "$UI" >/dev/null
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/modules" "$STAGE/ui" "$STAGE/menus"
cp "$MOD/mod_ui_engine.so" "$MOD/mod_ui_menu.so" "$MOD/mod_ui_vendor.so" \
   "$TEST/mod_sim_display.so" "$STAGE/modules/"
cp "$UI/settings_top.rgba" "$UI/settings_cat.rgba" "$UI/settings_rail.rgba" \
   "$UI/sublauncher_card_0.rgba" "$UI/sublauncher_card_1.rgba" \
   "$UI/sublauncher_card_2.rgba" "$STAGE/ui/"
cp "$ROOT/openaurora/menus/sub_launcher.json" "$STAGE/menus/"
cat >"$STAGE/config" <<EOF
autoload=
ui.dir=$STAGE/ui
ui.screen=sub_launcher
ui.paint=1
ui.text=0
ui.fps=0
ui.ok_file=
ui.sub_sel=-1
ui.sub_progress=0
ui_menu.file=$STAGE/menus/sub_launcher.json
EOF

run_case() {
    sel=$1
    progress=$2
    out=$3
    sed -i "s/^ui.sub_sel=.*/ui.sub_sel=$sel/" "$STAGE/config"
    sed -i "s/^ui.sub_progress=.*/ui.sub_progress=$progress/" "$STAGE/config"
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
    cp "$STAGE/frame.0" "$STAGE/$out"
}

run_case -1 0 frame-static
run_case 2 0 frame-p0
run_case 2 0.5 frame-p0.5
run_case 2 1 frame-p1

python3 - "$STAGE" "$UI/settings_top.rgba" <<'PY'
import struct
import sys
from PIL import Image

stage, oracle = sys.argv[1], sys.argv[2]
expect = {
    "0": (701, 276, 936, 406),
    "0.5": (701, 276, 977, 428),
    "1": (701, 276, 1018, 451),
}

def frame(path):
    data = open(path, "rb").read()
    if len(data) != 1920 * 1080 * 4:
        raise SystemExit(f"bad frame size: {path}")
    return data

def band(data):
    return data[240 * 1920 * 4:(240 + 180) * 1920 * 4]

vendor = open(oracle, "rb").read()
w, h = struct.unpack_from("<II", vendor, 4)
vendor = vendor[12:]
if (w, h) != (1920, 180):
    raise SystemExit("unexpected settings_top dimensions")
static = band(frame(f"{stage}/frame-static"))
ae = sum(abs(static[i] - vendor[i]) for i in range(len(vendor)))
if ae:
    raise SystemExit(f"static band mismatch: AE={ae}")

for p in ("0", "0.5", "1"):
    data = frame(f"{stage}/frame-p{p}")
    im = Image.frombytes("RGBA", (1920, 1080), data)
    px = im.load()
    xs, ys = [], []
    for y in range(270, 470):
        for x in range(690, 1100):
            r, g, b, a = px[x, y]
            if 22 <= r <= 80 and g <= 45 and b <= 60:
                xs.append(x)
                ys.append(y)
    got = (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None
    if got != expect[p]:
        raise SystemExit(f"p={p}: card2 bbox {got}, expected {expect[p]}")
print("ok qemu sub-launcher: static AE=0; "
      "300 ms card2 701,276 235x130 -> 701,276 317x175")
PY
