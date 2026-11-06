#!/bin/sh
# Offline QEMU check for the live localized sub-launcher text and the runtime
# key/timer animation. No TV or fbdev access.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-sub-live}
UI=$ROOT/artifacts/workbench/source-stage-20261101/ui
FONT=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data
PROFILE=$ROOT/artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin
MOD=$ROOT/openaurora/build-tv-v3/modules
TEST=$ROOT/openaurora/build-tv-v3/tests
HOST=$ROOT/openaurora/build-tv-v3/openaurora

if [ "${OA_QEMU_SKIP_BUILD:-0}" != 1 ]; then
    make -C "$ROOT/openaurora" -j"${JOBS:-4}" TARGET=tv BUILD=build-tv-v3 CC="$ARM" \
        build-tv-v3/openaurora \
        build-tv-v3/tests/mod_sim_display.so \
        build-tv-v3/modules/mod_ui_engine.so \
        build-tv-v3/modules/mod_ui_menu.so \
        build-tv-v3/modules/mod_i18n.so \
        build-tv-v3/modules/mod_ticker.so \
        build-tv-v3/modules/mod_ui_clock.so \
        build-tv-v3/modules/mod_ir.so \
        build-tv-v3/modules/mod_ui_vendor.so >/dev/null
fi

if [ "${OA_QEMU_SKIP_BUILD:-0}" != 1 ] || [ ! -f "$TEST/mod_plat_stub.so" ]; then
    "$ARM" -I"$ROOT/openaurora/include" -DOA_NO_VENDOR -D_GNU_SOURCE \
        -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic -fPIC -shared \
        -o "$TEST/mod_plat_stub.so" "$ROOT/openaurora/modules/plat/plat.c"
fi

for f in "$HOST" "$TEST/mod_sim_display.so" "$MOD/mod_ui_engine.so" \
         "$MOD/mod_ui_menu.so" "$MOD/mod_i18n.so" "$MOD/mod_ticker.so" \
         "$MOD/mod_ui_clock.so" "$MOD/mod_ir.so" "$TEST/mod_plat_stub.so" \
         "$MOD/mod_ui_vendor.so" \
         "$UI/settings_top.rgba" "$UI/settings_cat.rgba" \
         "$UI/settings_rail.rgba" "$FONT.txt" "$FONT.bin" "$PROFILE" \
         "$ROOT/openaurora/menus/sub_launcher.json"; do
    test -f "$f" || { echo "missing $f" >&2; exit 2; }
done

if [ ! -f "$UI/sublauncher_card_0_bg.rgba" ]; then
    python3 "$ROOT/openaurora/tools/mk-sublauncher-cards.py" \
        "$UI/settings_top.rgba" "$UI" >/dev/null
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/modules" "$STAGE/ui" "$STAGE/menus"
cp "$TEST/mod_sim_display.so" "$MOD/mod_ui_engine.so" "$MOD/mod_ui_menu.so" \
   "$MOD/mod_i18n.so" "$MOD/mod_ticker.so" "$MOD/mod_ui_clock.so" \
   "$MOD/mod_ir.so" "$MOD/mod_ui_vendor.so" "$STAGE/modules/"
cp "$TEST/mod_plat_stub.so" "$STAGE/modules/mod_plat.so"
cp "$UI/settings_top.rgba" "$UI/settings_cat.rgba" "$UI/settings_rail.rgba" \
   "$UI/sublauncher_card_0.rgba" "$UI/sublauncher_card_1.rgba" \
   "$UI/sublauncher_card_2.rgba" "$UI/sublauncher_card_0_bg.rgba" \
   "$UI/sublauncher_card_1_bg.rgba" "$UI/sublauncher_card_2_bg.rgba" "$STAGE/ui/"
cp "$ROOT/openaurora/menus/sub_launcher.json" "$STAGE/menus/"
cat >"$STAGE/config" <<EOF
autoload=
ui.dir=$STAGE/ui
ui.screen=sub_launcher
ui.paint=1
ui.text=1
ui.fps=0
ui.ok_file=
ui.sub_sel=-1
ui.sub_progress=0
ui.font_index=$FONT.txt
ui.font_pack=$FONT.bin
i18n.profile=$PROFILE
i18n.language=dut
plat.enable=1
ir.device=
ir.blast=0
ui_menu.file=$STAGE/menus/sub_launcher.json
EOF

OA_QEMU_MODULES="sim_display ui_engine ticker ui_clock plat ir ui_menu i18n ui_vendor" \
OA_QEMU_KEY=22 OA_QEMU_HOLD_MS=700 \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null

python3 - "$STAGE" "$UI/settings_top.rgba" <<'PY'
import glob
import struct
import sys
from PIL import Image

stage, oracle = sys.argv[1], sys.argv[2]
capd = open(oracle, "rb").read()
w, h = struct.unpack_from("<II", capd, 4)
cap = Image.frombytes("RGBA", (w, h), capd[12:])
cp = cap.load()

# Expected text ink at p0, relative to each card (x, y, x2, y2). These are
# measured from the same vendor capture; the live render must stay close.
expected = {
    0: ((86, 19, 147, 37), (52, 76, 184, 109)),
    1: ((34, 19, 198, 42), (52, 76, 181, 102)),
    2: ((48, 19, 184, 37), (87, 77, 141, 102)),
}

def ink(px, x0, x1, y0, y1):
    xs, ys = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b, a = px[x, y]
            if max(r, g, b) > 120:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None

frame0 = Image.frombytes("RGBA", (1920, 1080), open(f"{stage}/frame.0", "rb").read())
fp = frame0.load()
for card in range(3):
    x0 = 201 + card * 250
    label = ink(fp, x0, x0 + 235, 290, 320)
    value = ink(fp, x0, x0 + 235, 345, 395)
    if not label or not value:
        raise SystemExit(f"card {card}: missing live text")
    for name, got, want in (("label", label, expected[card][0]),
                            ("value", value, expected[card][1])):
        rel = (got[0] - x0, got[1] - 276, got[2] - x0, got[3] - 276)
        if any(abs(rel[i] - want[i]) > 4 for i in range(4)):
            raise SystemExit(f"card {card} {name}: live {rel}, vendor {want}")

def card_bottom(path):
    im = Image.frombytes("RGBA", (1920, 1080), open(path, "rb").read())
    px = im.load()
    last = None
    for y in range(270, 470):
        n = 0
        for x in range(250, 350):
            r, g, b, a = px[x, y]
            if abs(r - 33) <= 4 and abs(g - 21) <= 4 and abs(b - 28) <= 4:
                n += 1
        if n > 20:
            last = y
    return last

frames = sorted(glob.glob(f"{stage}/frame.*"),
                key=lambda p: int(p.rsplit(".", 1)[1]))
seq = [card_bottom(p) for p in frames]
if not seq or seq[0] != 405:
    raise SystemExit(f"unexpected static card bottom: {seq[:1]}")
if seq[-1] not in (450, 451):
    raise SystemExit(f"animation did not reach the scaled bottom: {seq[-1]}")
if len(set(seq)) < 6:
    raise SystemExit(f"too few animation steps: {seq}")
print("ok qemu sub-launcher live: Dutch text bboxes within 4 px; "
      f"key 22 timer card bottom {seq[0]} -> {seq[-1]} over {len(seq)} frames")
PY
