# Offline QEMU UI oracle and JSON sub-launcher row — 2026-11-06

The TV was off limits for this step, so the layout was verified entirely
offline: the real ARM UI stack runs under `qemu-arm-static` with a framebuffer
display provider, and the p0/03 oracle image is compared byte-for-byte. No TV
command, fb1 access, input device or vendor process was touched.

## QEMU display provider

`tools/display/sim-display.c` is a self-contained `display` service provider:

- 1920x1080 RGBA canvas, integer source-over alpha, nearest-neighbour
  `blit_scaled` (no libm dependency);
- `present` writes `$OA_NAV_CAPTURE.<seq>` as top-down RGBA;
- publishes `display` v1 and `display.alpha`;
- ARM build max GLIBC 2.4, so it runs against the TV's glibc 2.23 rootfs.

`tools/qemu-ui-layout.sh STAGE_DIR` starts the real
`build-tv-v3/openaurora` under `qemu-arm-static -L rootfs`, loads
`sim_display`, `ui_engine`, `ui_menu` and `ui_vendor` through the control
socket, and captures frame 0. It never opens a TV device.

The p0/03 `SCREEN_SETTINGS` band was the first oracle:

```text
frame band y=240..420 vs settings_top.rgba
AE=0  max=0  changed=0/345,600
```

This is the real ARM `ui_vendor`/`ui_engine` stack, not the host recorder.

## Checked sub-launcher row

`DialogSubLauncher.svg` listBox is 189,255 1774x222 and the item resource is
240x140. The p0/03 `settings_top` capture was measured for the runtime card
content:

```text
cards          201/451/701, band y=36 -> screen y=276, 235x130
pitch          250
item cell      199,274 240x140
```

`menus/sub_launcher.json` now carries this as a `layout` section, explicitly
sourced from the capture, not guessed:

```json
"layout": {
  "source": "p0/03 settings_top capture ...",
  "card": [201, 276, 235, 130],
  "pitch": 250,
  "item": [199, 274, 240, 140]
}
```

`oa_menu_card_rect` uses the layout and the recovered item scale range
(1→1.35) to return the card and item rects. The enlarged scale still comes
from `sublauncheritemFA`; the runtime row origin comes from the checked
capture. `SVGListBoxBase::renderBackground` reads standard names 43
(`rowPos`) and 44 (`scrollPos`), and `calculateHeight` sets the horizontal
step from the `mRowR`/`mRowM` widths; that confirms the source of the 240 px
item step, while the exact mask reduction remains a separate derivation.

`ui_vendor` now has `SCREEN_SUBLAUNCHER` / `ui.screen=sub_launcher`; the
`open:sub_launcher` action selects it. The screen:

- paints the captured p0/03 bands,
- looks up `ui.menu`, takes the picture branch and maps
  `MODE`, `ENERGY_SAVING`, `PICTURE_ZOOM` to the three captured card assets,
- leaves the vendor row untouched when unfocused, and
- scales the selected card from 235x130 to 317x175 using the lab
  `ui.sub_sel` / `ui.sub_progress` knobs.

The card cutouts are made by `tools/mk-sublauncher-cards.py` from
`settings_top.rgba`; they are the vendor's own pixels, not synthesized tiles.

## Module symbol visibility

`ui_menu` and `ui_vendor` both link `common/menu_model.c`. The two module
rules now build with `-fvisibility=hidden`, and `OA_MODULE_EXPORT` marks only
`oa_module_entry` as default-visible. This prevents one module from
interposing the other's `oa_menu_*`/`oa_json_*` symbols and leaves only the
loader entry point in the dynamic symbol table:

```text
nm -D --defined-only build/modules/mod_ui_vendor.so  -> oa_module_entry
nm -D --defined-only build/modules/mod_ui_menu.so    -> oa_module_entry
```

## Oracle results

`tools/qemu-sub-launcher-check.sh` builds the ARM stack, stages the modules,
assets and JSON, runs four QEMU captures, and checks:

```text
static row vs settings_top:  AE=0  changed=0/345,600
card 2 p=0.0: 701,276 .. 936,406   (235x130)
card 2 p=0.5: 701,276 .. 977,428   (276x152)
card 2 p=1.0: 701,276 .. 1018,451  (317x175)
```

The growth matches the recovered `#scalerect` 1→1.35 range and truncation.
The static row is byte-identical to the p0/03 oracle.

## Verification

```text
ok qemu sub-launcher: static AE=0; 300 ms card2 701,276 235x130 -> 701,276 317x175
full_check=0
```

The JSON/model and service tests still pass:

```text
ok menu model: JSON firmware tree, actions and 300 ms item animation
ok ui.menu service: JSON tree, reload, and failed-load isolation
ok ui.menu action: JSON entry invokes the vendor screen dispatcher
```

## Limits

- The focused-card state has no vendor capture, so the 300 ms samples are
  checked against the firmware geometry, not a vendor transition. The static
  unfocused row is the exact oracle.
- The card text is baked into the captured cutouts, not live settings text.
  Live labels/values and the glyph motion remain.
- `ui.sub_sel`/`ui.sub_progress` are lab knobs; the runtime key handler and
  300 ms repaint timer are not wired to the sub-launcher screen yet.
- The QEMU provider is a layout oracle, not scanout/Mali parity; `ui_engine`
  warns that `display` v1 is below its v2 request, and the settings screen
  uses the v1 path.
- No TV run was made.
