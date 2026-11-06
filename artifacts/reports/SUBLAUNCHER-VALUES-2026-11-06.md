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
- `NovoPictureSettings::getZoomMode(index, format)` reads **+0x0c** for
  format 2, **+0x08** for format 1, and for format 0 (the UI path) reads
  **+0x0c** only when `isFullModeApplicable(index)`, otherwise **+0x08**.
  `setZoomMode` writes the same field, except that format 0 also uses +0x0c
  for `ZOOM_MODE_FULL` (9).
- `getEnergySavingMode(source, pictureMode, device3d)` reads
  **+0x44 + pictureMode*0x70 + device3d*0x38** inside the same 0x240-byte block.

`settingsDB.Settings.m_picture_setting_configuration` is exactly 11520 bytes =
20 sources × 0x240. The fields store the vendor `SettingValue` IDs, not JSON
array positions. Recovered IDs:

```text
MODE          DYNAMIC 0, NATURAL 1, CINEMA 2, GAME 3, SPORT 4
ENERGY_SAVING OFF 0, MINIMUM 1, MEDIUM 2, MAXIMUM 3, AUTO 4,
              SCREEN_OFF 6, CUSTOM 5
PICTURE_ZOOM  AUTO 1, 16:9 5, SUBTITLE 6, 14:9 3, 14:9 ZOOM 4,
              4:3 2, 21:9 11, CINEMA 8, FULL 9
```

The dump's source 1 has mode 1 (NATURAL), zoom 1 (AUTO, not 16:9) and energy
3 (MAXIMUM). The p0/03 capture (NATURAL / 16:9 / MAXIMUM) matches configs 3,
4 and 14, which store zoom 5. The dump's `m_last_zoom_mode` is 5 (16:9), and
`m_current_source` is 5 (whose config is DYNAMIC / AUTO-or-FULL / OFF). The
QEMU check therefore uses source 3 for the capture values and source 5 for
the current-source case.

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

- on start/rebind it derives the configuration index from
  `Settings.m_current_source`: `InputManager::setCurrentExtSrc` stores the
  external interface index and writes `setCurrentSource(index+1)`;
  `getPictureSettingsIndexForCurrentSource` returns the same index+1 for
  external sources and 0 for the tuner. So `picture index == m_current_source`
  (0 = TV). An explicit `picture.source` is a diagnostic override only;
- `set:<key>=<value>` and Left/Right write through `set_mode`/`set_zoom`/
  `set_energy`;
- changing MODE re-reads the energy stored for that picture mode, matching
  `NovoPictureSettings::setPictureMode` applying the mode's saved values.

The old `KEY=index` state file and its code are gone. A missing
`picture.settings` provider leaves the p0/03 defaults in place.

## Source selection

The Bronlijst rows map to the ICE descriptor index, not the row number. The
descriptor table at 0x0175de8c has groups at +0x28: CVBS index2, HDMI1..3
indices4..6, YPbPr 8, VGA/PC 9, WiDi 11, APM 12. `getFirstHDMIIndex` returns 4
and `setCurrentExtSrc(4)` writes `m_current_source=5`. `ui_vendor` now has
`SRC_PICTURE_INDEX = {0,3,5,6,7,9,10,12,13}` for the nine rows; selecting a row
writes `m_current_source` through `settings` and calls `picture_sync_index` so
the cards switch to that source's 0x240 slot immediately.

## Verification

Host, against a copy of the shipped settingsDB:

```text
ok picture.settings: vendor field IDs, 0x240 stride, full-mode zoom rule
picture object: 20 source configs, 80 zoom formats, 200 energy cases, 242 setters
ok picture object: vendor UserPreferences/NovoPictureSettings accessors match the model
```

This asserts source1 raw mode=1, zoom=1 (AUTO), energy(mode1)=3, source0
mode=0 and energy=0, writes CINEMA/4:3/AUTO, reloads them, rejects
out-of-range fields and refuses writes in read-only mode. It also checks the
full-mode rule: with `picture.full_mode=0`, `set_zoom(1,5)` writes +0x08 and
`set_zoom(1,9)` writes +0x0c while the getter still reads +0x08.

QEMU, with the real ARM `mod_settings`, the vendor `libsqlite3.so.0` and a
writable copy of the DB:

```text
ok qemu sub-launcher value: m_current_source=3 (capture config) gives
NATURAL/MAXIMUM/16:9 (132/129/54); 16:9 -> 155 (DB id 6), survives restart;
setting: focus bottom 450; Right AUTO width 176; MODE Cinema width 105,
energy OFF width 36; source5 gives DYNAMIC/OFF (151/36); source-screen
HDMI1 writes 5 and resyncs (151/36)
```

The default widths 132/129/54 are the p0/03 vendor capture's card 0/1/2
value ink widths, so the firmware DB's source-3 values drive the live row.

Steps: `m_current_source=3` renders NATURAL/MAXIMUM/16:9 (132/129/54 px);
`ui-action set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE` stores the vendor id 6 at
blob `+0x0c` and renders 155 px; a fresh process renders the same;
`setting:ENERGY_SAVING` reaches the 450 px scaled bottom; `Down,Down,Right`
writes energy id 4 and renders AUTO; `set:MODE=CINEMA` writes mode 2 and
applies that mode's energy 0, rendering Cinema and Uit; setting
`m_current_source=5` in the DB and restarting selects config 5 and renders
DYNAMIC/OFF (151/36 px). Finally the source screen starts at HDMI1 row 2,
Select writes `m_current_source=5` through the settings service, and
`open:sub_launcher` shows DYNAMIC/OFF (151/36 px) in the same process; the DB
assertion confirms the stored value is 5.

The existing text/oracle checks still pass:

```text
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
full_check=0
```

## Limits

- The service reads/writes the **serialized** settingsDB picture blob. The
  vendor `UserPreferences`/`NovoPictureSettings` accessors are now executed
  offline against that blob (PICTURE-OBJECT-2026-11-06); the running vendor
  object and NVRAM-backed instance are still not executed. The separate
  `m_picture_mode_preset_key` and `m_last_zoom_mode` columns are not updated.
- The configuration index is derived from `m_current_source`, matching the
  firmware identity, and the Bronlijst Select writes it and re-syncs the
  cards. The select also calls `mod_source`/`Hal_Vin_SelectInput` behind
  `source.enable`; real `/dev/ntkdvif`/TV hardware switching remains a separate
  unaccepted gate. `InputManager::getCurrentExtSrc` itself is not called. The
  row mapping is the ICE descriptor table above; a runtime-disabled or
  reordered interface list would need the same source-list service.
- HDR energy variants (`+0x446c`, `+0xa10`, `+0xa48`) are not executed; the
  non-HDR config array is used. The format-0 full-mode decision is exposed as
  `picture.full_mode` (default 1), not derived from the live panel/decode
  state.
- Writing the TV's real `/conf/sqlite_db/settingsDB.db` requires
  `settings.readonly=0`; stock Aurora caches these values, so this is a
  deliberate deployment decision, not the default.
- The vendor selection-box value animation is not reproduced.
- No vendor value-change capture exists while the TV is off limits; validation
  is the firmware DB values, rendering and the font audit.
