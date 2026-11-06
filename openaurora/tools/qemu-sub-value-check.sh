#!/bin/sh
# Offline QEMU check for the sub-launcher value action and persistence:
# set:PICTURE_ZOOM changes the live card value, ui.state_file records the
# index, and a fresh process loads it again. No TV or fbdev access.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ARM=${ARM:-$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
STAGE=${OA_QEMU_STAGE:-/tmp/oa-qemu-sub-value}
UI=$ROOT/artifacts/workbench/source-stage-20261101/ui
FONT=$ROOT/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data
PROFILE=$ROOT/artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin
MOD=$ROOT/openaurora/build-tv-v3/modules
TEST=$ROOT/openaurora/build-tv-v3/tests
HOST=$ROOT/openaurora/build-tv-v3/openaurora
MODULES="sim_display ui_engine ticker ui_clock plat ir ui_menu i18n ui_vendor"

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
         "$MOD/mod_ui_vendor.so" "$UI/settings_top.rgba" \
         "$UI/settings_cat.rgba" "$UI/settings_rail.rgba" "$FONT.txt" \
         "$FONT.bin" "$PROFILE" \
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
ui.state_file=$STAGE/state.conf
i18n.profile=$PROFILE
i18n.language=dut
plat.enable=1
ir.device=
ir.blast=0
ui_menu.file=$STAGE/menus/sub_launcher.json
EOF

last_frame() {
    ls "$STAGE"/frame.* 2>/dev/null | sed 's/.*\.//' | sort -n | tail -1
}

# 1. Default p0/03 value (16:9) in a fresh state.
OA_QEMU_MODULES="$MODULES" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
cp "$STAGE/frame.0" "$STAGE/base.rgba"

# 2. Control-socket set: action; the later frames carry the new value.
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=500 \
OA_QEMU_ACTION="set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
test -n "$n" || { echo "no action frame" >&2; exit 3; }
cp "$STAGE/frame.$n" "$STAGE/changed.rgba"
grep -q '^PICTURE_ZOOM=2$' "$STAGE/state.conf" || {
    echo "state file did not record PICTURE_ZOOM=2" >&2; cat "$STAGE/state.conf" >&2; exit 4; }

# 3. Fresh process must load the saved index.
OA_QEMU_MODULES="$MODULES" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
cp "$STAGE/frame.0" "$STAGE/restart.rgba"

# 4. setting: action focuses the requested card and runs the 300 ms animation.
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=700 \
OA_QEMU_ACTION="setting:ENERGY_SAVING" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
cp "$STAGE/frame.$n" "$STAGE/focus.rgba"

# 5. Up/Down focus the card; Right cycles the focused value (vendor
#    SVGSelectionBox cyclic default). Down,Down -> ENERGY_SAVING, Right ->
#    MAXIMUM_STR(3) -> AUTO(4).
rm -f "$STAGE/state.conf"
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=400 OA_QEMU_KEYS="20,20,22" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
cp "$STAGE/frame.$n" "$STAGE/cycle.rgba"
grep -q '^ENERGY_SAVING=4$' "$STAGE/state.conf" || {
    echo "Right did not cycle MAXIMUM_STR to AUTO" >&2; cat "$STAGE/state.conf" >&2; exit 5; }

python3 - "$STAGE" <<'PY'
import glob
import sys
from PIL import Image

stage = sys.argv[1]

def load(path):
    return Image.frombytes("RGBA", (1920, 1080), open(path, "rb").read()).load()

def ink(px, x0, x1, y0, y1):
    xs, ys = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b, a = px[x, y]
            if max(r, g, b) > 120:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None

def value_box(px):
    x0 = 199 + 2 * 250
    return ink(px, x0, x0 + 240, 345, 400)

base = value_box(load(f"{stage}/base.rgba"))
changed = value_box(load(f"{stage}/changed.rgba"))
restart = value_box(load(f"{stage}/restart.rgba"))
if not base or not changed or not restart:
    raise SystemExit("missing card 2 value text")
bw = base[2] - base[0]
cw = changed[2] - changed[0]
if not (40 <= bw <= 70):
    raise SystemExit(f"default 16:9 width unexpected: {bw}")
if cw < 120:
    raise SystemExit(f"set: value did not change to the long label: width {cw}")
if changed != restart:
    raise SystemExit(f"persisted value differs after restart: {changed} vs {restart}")

focus = load(f"{stage}/focus.rgba")
def card_bottom(px, x0):
    last = None
    for y in range(270, 470):
        n = 0
        for x in range(x0 + 50, x0 + 150):
            r, g, b, a = px[x, y]
            if abs(r - 33) <= 4 and abs(g - 21) <= 4 and abs(b - 28) <= 4:
                n += 1
        if n > 20:
            last = y
    return last
bottom = card_bottom(focus, 199 + 250)
if bottom not in (450, 451):
    raise SystemExit(f"setting: focus card did not reach the scaled bottom: {bottom}")

cycle = load(f"{stage}/cycle.rgba")
ebox = ink(cycle, 199 + 250, 199 + 250 + 240, 345, 400)
if not ebox or ebox[2] - ebox[0] < 160:
    raise SystemExit(f"Right did not render the long AUTO value: {ebox}")
print(f"ok qemu sub-launcher value: 16:9 width {bw} -> {cw}, "
      f"PICTURE_ZOOM=2 persisted across restart; setting: focus bottom {bottom}; "
      f"Right AUTO width {ebox[2] - ebox[0]}")

PY
