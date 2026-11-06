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
| `picture-mode-names.txt` | `PictureModeNameProvider`: 5 modes, name table filled at runtime |
| `demoapp-ctor.txt` | `DemoApp` constructor: `+0x318` starts zeroed |

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
- `MODE` is dynamic. `PictureModeNameProvider::getNumberOfPictureModes`
  returns 5 and `getNameStrIdOfPictureMode(i)` returns the fourth byte of a
  runtime-filled 8-byte table entry; the JSON carries the `picture_modes`
  marker instead of a list.

`SVGSelectionBox`'s constructor writes 0x1 at `+0x2f4`, so `onKeyLeft`/
`onKeyRight` wrap (cyclic navigation) rather than clamp.
