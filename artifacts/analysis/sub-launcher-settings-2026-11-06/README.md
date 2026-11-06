# Sub-launcher settings backend — 2026-11-06

TV-offline Ghidra evidence for the object behind `SubLauncherDialog`'s
`this+0x4a8 -> +0x318` calls. Program: `aurora.elf` MD5
`64c252340d3af6c5d66072611b7bad7a`.

| file | scope |
|---|---|
| `demoapp-modules.txt` | `DemoApp::createApplicationModules` (0x00cd5a98): `+0x318 = NovoSystemInitialiser::pictureSettings` |
| `picture-settings-getter.txt` | `NovoSystemInitialiser::pictureSettings` (0x00f21700), field `+0x2e8` |
| `easi-tv-ctor.txt` | `EASITVControlInterface` constructor; it is a data holder, not the `+0x318` object |
| `settings-methods-decompile.txt` | `NovoPictureSettings` get/set picture mode, zoom and energy methods |
| `preferences-decompile.txt` | `UserPreferences` picture configuration array and energy-saving fields |
| `picture-mode-names.txt` | `PictureModeNameProvider`: 5 modes; the table is filled by the initializer at 0x006eee50 |
| `picturemode-fields.txt` | `UserPreferences::get/setPictureMode`: config +0x04 |
| `picture-config-fields.txt` | `NovoPictureSettings::get/setPictureMode`; config/energy callers |
| `demoapp-ctor.txt` | `DemoApp` constructor: `+0x318` starts zeroed |
| `source-mapping.txt`, `source-mapping2.txt`, `source-mapping3.txt` | `InputManager::getCurrentExtSrc`/`setCurrentExtSrc`, external-source display state and `getFirstExtSource` |
| `ice-external-names.txt` | `ICE_ExternalGetName`/`ICE_ExternalNumberOfInterfaces` (13-entry descriptor table) |

## Object and vtable

`DemoApp+0x318` is a `NovoPictureSettings*`:

- `createApplicationModules` calls `NovoSystemInitialiser::pictureSettings`
  (getter returns `NovoSystemInitialiser+0x2e8`) and stores it at `DemoApp+0x318`.
- `NovoSystemInitialiser::initialise` at 0x00f28264 constructs a 0x90-byte
  `NovoPictureSettings` and writes it to `+0x2e8`.

The exported vtable symbol is `_ZTV19NovoPictureSettings` at 0x01518bb0. On
this ARM EABI the object vptr points 8 bytes after the symbol (after
offset-to-top and typeinfo). With that correction every call in
`SubLauncherDialog::update*` resolves exactly:

| vtable slot | method |
|---|---|
| 0x9c | `setPictureModeForCurrentSource(NovoPictureMode)` |
| 0xa4 | `applyPictureSettings(int,bool)` |
| 0xb0 | `getZoomMode(int,NovoVideoContentFormat)` |
| 0xb4 | `setZoomMode(int,NovoVideoTVZoomMode,NovoVideoContentFormat)` |
| 0xd0 | `getPictureSettingsIndexForCurrentSource()` |
| 0xe8 | `getPictureModeForCurrentSource()` |
| 0x100 / 0x104 | `getBacklightLevel` / `setBacklightLevel` |
| 0x194 / 0x198 / 0x19c | `getEnergySavingMode` / `getDefaultEnergySavingMode` / `setEnergySavingMode` |

`applyPictureSettings(int,bool)` takes the index returned by slot 0xd0 and a
save flag; `updatePictureMode` passes `(index, 0)`. `updateZoomMode` reads and
writes with content format 0. `updateEnergySaving` uses the backlight pair for
energy mode 5.

## Persistence

The picture methods do not touch SQLite directly. They read/write
`UserPreferences`:

- `getPictureSettingConfiguration(index)` returns `UserPreferences+0x43f0 +
  index*0x240`; the setter `memcpy`s 0x240 bytes and also updates a twin-source
  index.
- Energy saving is stored at `+0x4434 + pictureMode*0x240 +
  device3D*0x70 + sourceIndex*0x240` (or the HDR variants at `+0x446c`,
  `+0xa10`, `+0xa48`).
- `settingsDB` has the serialized blobs `m_picture_setting_configuration` and
  `m_last_zoom_mode`, plus integer columns such as `m_default_zoom_mode`;
  the exact UserPreferences-to-DB serialization was not executed.

## Value lists

- `ENERGY_SAVING` and `PICTURE_ZOOM` have explicit JSON arrays decoded from
  `initialisePictureSettingsShortcuts`; the selection-box item IDs are the
  picture/zoom enum indices.
- `MODE` comes from the runtime table. `getNumberOfPictureModes` returns 5;
  the initializer at 0x006eee50 fills 0x0178d64c with
  `[0] DYNAMIC, [1] NATURAL, [2] CINEMA, [3] GAME, [4] SPORT` from the
  literals at 0x6eee98..0x6eeea8. The JSON now carries that list; `set:MODE`
  maps the value index directly to `NovoPictureMode`.
- `m_picture_setting_configuration` is 20 x 0x240 bytes. Source 1 (HDMI1) has
  mode 1 (NATURAL) at +0x04, zoom 1 (16:9) at +0x0c, and energy 3 (MAXIMUM)
  for picture mode 1 at +0x44 + 1*0x70, matching the p0/03 capture.

`SVGSelectionBox`'s constructor writes 0x1 at `+0x2f4`, so `onKeyLeft`/
`onKeyRight` wrap (cyclic navigation) rather than clamp.

## Picture source index

`InputManager::setCurrentExtSrc(index, main)` stores the external interface
index at `+0x68` (main) and calls `UserPreferences::setCurrentSource(index+1)`.
`NovoPictureSettings::getPictureSettingsIndexForCurrentSource` returns
`getCurrentExtSrc(true)+1` for external sources and 0 for the tuner (or
`numExt+1` for radio). The picture-configuration index is therefore the
persisted `m_current_source`; the UI row order itself is not the index.
