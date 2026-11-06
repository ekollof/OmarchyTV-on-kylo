# Sub-launcher value actions and the firmware picture store — 2026-11-06

TV was off limits. This is firmware decoding plus offline host/QEMU checks: the
real `ui_vendor`/`ui_menu`/`i18n`/`settings` modules, the shipped langprofile,
the shipped settingsDB and the p0/03 card capture. No TV, fbdev, input device or
stock process was touched.

## Picture-mode list

`PictureModeNameProvider::getNumberOfPictureModes` returns 5.
`getNameStrIdOfPictureMode(i)` reads `[table + i*8 + 4]`, where the table at
0x0178d64c is filled by the local initializer at **0x006eee50**:

```text
[0] DYNAMIC   [1] NATURAL   [2] CINEMA   [3] GAME   [4] SPORT
```

The five name pointers are the literals at 0x6eee98..0x6eeea8, stored into
entry offsets +4/+12/+20/+28/+36. An earlier read assumed CINEMA at index 1;
the actual store order is NATURAL at 1 and CINEMA at 2. That now matches the
firmware DB and the p0/03 capture.

The JSON `MODE` item carries these five values and `set:MODE=<key>` maps the
value index directly to `NovoPictureMode` (the vendor's `SettingValue` IDs are
the same index in the five-mode branch). `tools/re/menu-tree.py` emits the list
instead of the old `picture_modes` marker.

## Settings backend

`DemoApp+0x318` is a `NovoPictureSettings*`; the object vptr is
`_ZTV19NovoPictureSettings` **+8** on this ARM EABI. The `SubLauncherDialog`
calls resolve to:

```text
0x9c setPictureModeForCurrentSource   0xa4 applyPictureSettings(index,save)
0xb0/0xb4 get/setZoomMode(index,fmt)  0xd0 getPictureSettingsIndexForCurrentSource
0xe8 getPictureModeForCurrentSource   0x194/0x198/0x19c energy get/default/set
0x100/0x104 get/setBacklightLevel
```

Those methods persist through `UserPreferences`:

- `getPictureSettingConfiguration(source)` is `UserPreferences+0x43f0 + source*0x240`.
- `getPictureMode(source)` reads that block at **+0x04**.
- `getZoomMode(index, 0)` reads **+0x08** (full-mode field) or **+0x0c**
  (16:9/format field); the captured source has both at 1.
- `getEnergySavingMode(source, pictureMode, device3d)` reads
  **+0x44 + pictureMode*0x70 + device3d*0x38** inside the same 0x240-byte block.

`settingsDB.Settings.m_picture_setting_configuration` is exactly 11520 bytes =
20 sources × 0x240. The dump's source 1 (HDMI1) has mode **1 = NATURAL**, zoom
**1 = 16:9**, and energy **3 = MAXIMUM** for picture mode 1 — the three values
visible in the p0/03 capture. That is the cross-check for the recovered field
offsets and the mode list.

## Implementation

`mod_settings` now publishes **`picture.settings` v1**:

```text
get_mode/set_mode        get_zoom/set_zoom
get_energy/set_energy    is_writable
```

It loads the 20×0x240 blob at start, bounds source 0..19, mode 0..4 and
device3d 0..1, and writes the whole blob back with `UPDATE Settings SET
m_picture_setting_configuration=?`. Writes are refused with `EROFS` while
`settings.readonly=1` (the default). Reads are safe in both modes.

`ui_vendor` now uses that service instead of the removed `ui.state_file`
shadow:

- on start/rebind it reads mode, zoom and the per-mode energy for
  `picture.source` (default 1 = HDMI1, the captured configuration);
- `set:<key>=<value>` and Left/Right write through `set_mode`/`set_zoom`/
  `set_energy`;
- changing MODE re-reads the energy stored for that picture mode, matching
  `NovoPictureSettings::setPictureMode` applying the mode's saved values.

The old `KEY=index` state file and its code are gone. A missing
`picture.settings` provider leaves the p0/03 defaults in place.

## Verification

Host, against a copy of the shipped settingsDB:

```text
ok picture.settings: source1 NATURAL/16:9/MAXIMUM, writes persist at 0x240 stride
```

This asserts source1 mode=1, zoom=1, energy(mode1)=3, source0 mode=0 and
energy=0, writes CINEMA/SUBTITLE/AUTO, reloads them, rejects out-of-range
fields and refuses writes in read-only mode.

QEMU, with the real ARM `mod_settings`, the vendor `libsqlite3.so.0` and a
writable copy of the DB:

```text
ok qemu sub-launcher value: 16:9 width 54 -> 155 (DB field 2), survives
restart; setting: focus bottom 450; Right AUTO width 176; MODE Cinema width
105, energy OFF width 36
```

Steps: default source1 renders 16:9; `ui-action set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE`
writes blob `+0x0c = 2` and renders 155 px; a fresh process renders the same;
`setting:ENERGY_SAVING` reaches the 450 px scaled bottom; `Down,Down,Right`
writes energy 4 and renders AUTO; `set:MODE=CINEMA` writes mode 2 and applies
that mode's energy 0, rendering Cinema and Uit.

The existing text/oracle checks still pass:

```text
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
full_check=0
```

## Limits

- The service reads/writes the **serialized** settingsDB picture blob, not the
  live vendor `NovoPictureSettings` object or its NVRAM-backed
  `UserPreferences` instance. The field layout and values are verified against
  the firmware data, but a live vendor object was not executed. The separate
  `m_picture_mode_preset_key` and `m_last_zoom_mode` columns are not updated.
- `picture.source` defaults to 1 (HDMI1). Deriving it at runtime from
  `InputManager`/`EASITVControlInterface` is not implemented; on another input
  the wrong 0x240 slot would be shown.
- HDR energy variants (`+0x446c`, `+0xa10`, `+0xa48`) and the format-0
  `isFullModeApplicable` zoom choice are not implemented; the non-HDR +0x0c
  field is used.
- Writing the TV's real `/conf/sqlite_db/settingsDB.db` requires
  `settings.readonly=0`; stock Aurora caches these values, so this is a
  deliberate deployment decision, not the default.
- The vendor selection-box value animation is not reproduced.
- No vendor value-change capture exists while the TV is off limits; validation
  is the firmware DB values, rendering and the font audit.
