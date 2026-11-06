# Sub-launcher value cycling, actions and persistence — 2026-11-06

TV was off limits. This is firmware decoding plus offline ARM/QEMU checks: the
real `ui_vendor`/`ui_menu`/`i18n` modules, the shipped langprofile and the
existing p0/03 card capture. No TV, fbdev, input device or stock process was
touched.

## Backend decode

`SubLauncherDialog`'s `this+0x4a8 -> +0x318` is a `NovoPictureSettings*`.
`DemoApp::createApplicationModules` (0x00cd5a98) stores the object returned by
`NovoSystemInitialiser::pictureSettings`; `NovoSystemInitialiser::initialise`
constructs it at 0xf28264. The exported vtable symbol is
`_ZTV19NovoPictureSettings` at 0x01518bb0. On this ARM EABI the object vptr is
the symbol **+8**, which makes every call resolve:

```text
0xe8 getPictureModeForCurrentSource()       0x9c setPictureModeForCurrentSource()
0xd0 getPictureSettingsIndexForCurrentSource()  0xa4 applyPictureSettings(index,0)
0xb0 getZoomMode(index,format)              0xb4 setZoomMode(index,zoom,format)
0x194/0x198/0x19c get/default/setEnergySavingMode()
0x100/0x104 get/setBacklightLevel()
```

The picture methods persist through `UserPreferences`, not directly through
SQLite: `getPictureSettingConfiguration(index)` is
`UserPreferences+0x43f0 + index*0x240`, and the energy value is indexed by
source/picture mode/3D mode at `+0x4434` (HDR variants at `+0x446c`, `+0xa10`,
`+0xa48`). `settingsDB` carries the likely serialization blobs
`m_picture_setting_configuration` and `m_last_zoom_mode`; that mapping was not
executed.

`SVGSelectionBox`'s constructor writes 0x1 at `+0x2f4`, so Left/Right use
**cyclic** navigation. The vendor key flow is therefore:

- Up/Down move the card focus and call `startSelectAnimation` (300 ms);
- Left/Right change the focused card's value, wrapping at both ends;
- Select has no action on a selection-box shortcut card.

## Implementation

`ui_vendor` now stores a selected index per card (`sub_value`, in SUB_CARDS
order), initialised to the p0/03 state: MODE has no index, ENERGY_SAVING = 3
(`MAXIMUM_STR`), PICTURE_ZOOM = 1 (`ZOOM_MODE_16_9`). The value text comes from
the JSON item's value array instead of the hardcoded key.

- `setting:<key>` focuses that card and starts the real 300 ms timer.
- `set:<key>=<value>` selects the value, invalidates the text blobs, saves and
  repaints when the sub-launcher is visible. Unknown key/value returns
  `ENOENT`, a dynamic list (`MODE`) returns `ENOTSUP`.
- Left/Right call the same selector; Up/Down keep the focus animation.

Persistence is a small `KEY=index` file at `ui.state_file`,
default `/tmp/openaurora-sub-launcher.state` (boot-scoped). Point it at a
`/conf` path for reboot persistence. It is a replacement-owned shadow: it does
not yet drive `NovoPictureSettings`, `UserPreferences`, the PQ/video backend or
the vendor settingsDB. `MODE`'s `picture_modes` list is a runtime-filled
`PictureModeNameProvider` table (5 modes) and is not decoded, so MODE keeps the
p0/03 fallback and rejects `set:`.

The control socket has a new command:

```text
ui-action <action>      apply a JSON menu action (open:/setting:/set:)
```

## Verification

`tools/qemu-sub-value-check.sh` drives the real stack through the control
socket:

1. default frame: card 2 renders `16:9`, ink width 54 px;
2. `ui-action set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE`: ink width 155 px and the
   state file contains `PICTURE_ZOOM=2`;
3. a fresh process loads the state and renders the same 155 px value;
4. `ui-action setting:ENERGY_SAVING`: card 1 reaches the scaled bottom 450;
5. `key 20,20,22` (Down, Down, Right): ENERGY_SAVING 3 -> 4, the card renders
   `Automatisch` (176 px) and the state file contains `ENERGY_SAVING=4`.

```text
ok qemu sub-launcher value: 16:9 width 54 -> 155, PICTURE_ZOOM=2 persisted
across restart; setting: focus bottom 450; Right AUTO width 176
```

Host and oracle checks:

```text
ok ui.menu action: JSON entry invokes open/setting/set and persists the value
ok qemu sub-launcher live: Dutch text within 4 px of p0/03; key 20 timer card bottom 405 -> 450
ok qemu sub-launcher: static AE=0; 300 ms card2 701,276 235x130 -> 317x175
full_check=0
```

## Limits

- The persisted state is a replacement shadow, not the vendor
  `UserPreferences`/PQ/settingsDB path; no real picture mode, zoom or energy
  saving is applied to the video pipeline.
- MODE's `picture_modes` list is not decoded; `set:MODE=...` returns
  `ENOTSUP` and the card still shows the p0/03 `NATURAL`.
- The default state path is boot-scoped. A `/conf` path persists across reboot
  but is a persistent write and must be chosen deliberately.
- The vendor selection-box value animation is not reproduced; value changes
  repaint immediately.
- The value ellipsis path is still unexercised (values max 183 px < 240).
- No focused/multilingual vendor capture exists while the TV is off limits;
  validation is rendering plus the font audit.
