#!/bin/sh
# Offline QEMU check for the sub-launcher value actions and the firmware
# picture store: set: updates the card, settingsDB's 0x240-byte
# PictureSettingConfiguration changes, and a fresh process reads it back.
# No TV or fbdev access.
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
SETTINGSDB=$ROOT/artifacts/dumps/apdat_files/sqlite_db/settingsDB.db
SQLITE_LIB=$ROOT/artifacts/workbench/rootfs/vendor/lib/libs/libsqlite3.so.0
MODULES="sim_display ui_engine ticker ui_clock plat ir ui_menu i18n settings ui_vendor"

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
        build-tv-v3/modules/mod_settings.so \
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
         "$MOD/mod_settings.so" "$MOD/mod_ui_vendor.so" "$SETTINGSDB" \
         "$SQLITE_LIB" "$UI/settings_top.rgba" \
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
   "$MOD/mod_ir.so" "$MOD/mod_settings.so" "$MOD/mod_ui_vendor.so" \
   "$STAGE/modules/"
cp "$TEST/mod_plat_stub.so" "$STAGE/modules/mod_plat.so"
cp "$SQLITE_LIB" "$STAGE/libsqlite3.so.0"
cp "$SETTINGSDB" "$STAGE/picture.db"
python3 - "$STAGE/picture.db" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute("UPDATE Settings SET m_current_source=1")
db.commit()
PY
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
settings.db=$STAGE/picture.db
settings.readonly=0
i18n.profile=$PROFILE
i18n.language=dut
plat.enable=1
ir.device=
ir.blast=0
ui_menu.file=$STAGE/menus/sub_launcher.json
EOF

export LD_LIBRARY_PATH="$STAGE${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

last_frame() {
    ls "$STAGE"/frame.* 2>/dev/null | sed 's/.*\.//' | sort -n | tail -1
}

# 1. Default p0/03 values from the firmware DB: source1 NATURAL/MAXIMUM/16:9.
OA_QEMU_MODULES="$MODULES" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
cp "$STAGE/frame.0" "$STAGE/base.rgba"

# 2. Control-socket set: action; the later frames carry the new value and the
#    blob's zoom field must change.
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=500 \
OA_QEMU_ACTION="set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
test -n "$n" || { echo "no action frame" >&2; exit 3; }
cp "$STAGE/frame.$n" "$STAGE/changed.rgba"
python3 - "$STAGE/picture.db" <<'PY'
import sqlite3, struct, sys
b = sqlite3.connect(sys.argv[1]).execute(
    "select m_picture_setting_configuration from Settings").fetchone()[0]
assert struct.unpack_from("<i", b, 0x240 + 0x0c)[0] == 2, "zoom field not 2"
PY

# 3. Fresh process must load the saved blob.
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
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=400 OA_QEMU_KEYS="20,20,22" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
cp "$STAGE/frame.$n" "$STAGE/cycle.rgba"
python3 - "$STAGE/picture.db" <<'PY'
import sqlite3, struct, sys
b = sqlite3.connect(sys.argv[1]).execute(
    "select m_picture_setting_configuration from Settings").fetchone()[0]
off = 0x240 + 0x44 + 1 * 0x70
assert struct.unpack_from("<i", b, off)[0] == 4, "mode1 energy not AUTO"
PY

# 6. set:MODE=CINEMA writes the mode field and applies that mode's stored
#    energy (mode 2 -> OFF in the firmware DB).
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=400 \
OA_QEMU_ACTION="set:MODE=CINEMA" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
cp "$STAGE/frame.$n" "$STAGE/mode.rgba"
python3 - "$STAGE/picture.db" <<'PY'
import sqlite3, struct, sys
b = sqlite3.connect(sys.argv[1]).execute(
    "select m_picture_setting_configuration from Settings").fetchone()[0]
assert struct.unpack_from("<i", b, 0x240 + 0x04)[0] == 2, "mode field not CINEMA"
off = 0x240 + 0x44 + 2 * 0x70
assert struct.unpack_from("<i", b, off)[0] == 0, "mode2 energy not OFF"
PY

# 7. The picture source comes from m_current_source (firmware: external
#    interface index + 1). Source 5 selects config 5: DYNAMIC/OFF/16:9.
python3 - "$STAGE/picture.db" 5 <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute("UPDATE Settings SET m_current_source=?", (int(sys.argv[2]),))
db.commit()
PY
OA_QEMU_MODULES="$MODULES" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
cp "$STAGE/frame.0" "$STAGE/source5.rgba"

# 8. Source screen: HDMI1 is Bronlijst row 2 -> m_current_source 5. Selecting
#    it writes the setting through the service and re-syncs the cards in the
#    same process; open:sub_launcher then shows config 5 (DYNAMIC/OFF).
python3 - "$STAGE/picture.db" 1 <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute("UPDATE Settings SET m_current_source=?", (int(sys.argv[2]),))
db.commit()
PY
cp "$STAGE/config" "$STAGE/config.save"
sed -i 's/^ui.screen=.*/ui.screen=sources/' "$STAGE/config"
printf 'ui.src=2\n' >>"$STAGE/config"
OA_QEMU_MODULES="$MODULES" OA_QEMU_HOLD_MS=600 OA_QEMU_KEYS=53 \
OA_QEMU_ACTION="open:sub_launcher" \
    "$ROOT/openaurora/tools/qemu-ui-layout.sh" "$STAGE" >/dev/null
n=$(last_frame)
cp "$STAGE/frame.$n" "$STAGE/source.rgba"
mv "$STAGE/config.save" "$STAGE/config"
python3 - "$STAGE/picture.db" <<'PY'
import sqlite3, sys
value = sqlite3.connect(sys.argv[1]).execute(
    "select m_current_source from Settings").fetchone()[0]
assert value == 5, f"source screen wrote {value}, expected 5"
PY

python3 - "$STAGE" <<'PY'
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

def value_box(px, card):
    x0 = 199 + card * 250
    return ink(px, x0, x0 + 240, 345, 400)

base = value_box(load(f"{stage}/base.rgba"), 2)
changed = value_box(load(f"{stage}/changed.rgba"), 2)
restart = value_box(load(f"{stage}/restart.rgba"), 2)
if not base or not changed or not restart:
    raise SystemExit("missing card 2 value text")
bw = base[2] - base[0]
cw = changed[2] - changed[0]
if not (40 <= bw <= 70):
    raise SystemExit(f"default 16:9 width unexpected: {bw}")
# The default frame must show the other two DB values too: NATURAL and
# MAXIMUM (font12 ink widths for the Dutch strings).
v0 = value_box(load(f"{stage}/base.rgba"), 0)
v1 = value_box(load(f"{stage}/base.rgba"), 1)
if not v0 or not (122 <= v0[2] - v0[0] <= 145):
    raise SystemExit(f"default MODE is not NATURAL: {v0}")
if not v1 or not (118 <= v1[2] - v1[0] <= 140):
    raise SystemExit(f"default ENERGY is not MAXIMUM: {v1}")
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

source5 = load(f"{stage}/source5.rgba")
s0 = ink(source5, 199, 199 + 240, 345, 400)
s1 = ink(source5, 199 + 250, 199 + 250 + 240, 345, 400)
if not s0 or not (145 <= s0[2] - s0[0] <= 170):
    raise SystemExit(f"m_current_source=5 did not select DYNAMIC: {s0}")
if not s1 or not (25 <= s1[2] - s1[0] <= 55):
    raise SystemExit(f"m_current_source=5 did not select OFF energy: {s1}")

source = load(f"{stage}/source.rgba")
q0 = ink(source, 199, 199 + 240, 345, 400)
q1 = ink(source, 199 + 250, 199 + 250 + 240, 345, 400)
if not q0 or not (145 <= q0[2] - q0[0] <= 170):
    raise SystemExit(f"source-select did not resync DYNAMIC: {q0}")
if not q1 or not (25 <= q1[2] - q1[0] <= 55):
    raise SystemExit(f"source-select did not resync OFF: {q1}")

mode = load(f"{stage}/mode.rgba")
m0 = ink(mode, 199, 199 + 240, 345, 400)
m1 = ink(mode, 199 + 250, 199 + 250 + 240, 345, 400)
if not m0 or not (95 <= m0[2] - m0[0] <= 120):
    raise SystemExit(f"set:MODE did not render Cinema: {m0}")
if not m1 or not (25 <= m1[2] - m1[0] <= 55):
    raise SystemExit(f"mode2 energy did not render OFF: {m1}")
print(f"ok qemu sub-launcher value: m_current_source=1 gives "
      f"NATURAL/MAXIMUM/16:9 ({v0[2]-v0[0]}/{v1[2]-v1[0]}/{bw}); 16:9 -> {cw} "
      f"(DB field 2), survives restart; setting: focus bottom {bottom}; "
      f"Right AUTO width {ebox[2] - ebox[0]}; MODE Cinema width "
      f"{m0[2] - m0[0]}, energy OFF width {m1[2] - m1[0]}; source5 gives "
      f"DYNAMIC/OFF ({s0[2] - s0[0]}/{s1[2] - s1[0]}); source-screen HDMI1 "
      f"writes 5 and resyncs ({q0[2] - q0[0]}/{q1[2] - q1[0]})")
PY
