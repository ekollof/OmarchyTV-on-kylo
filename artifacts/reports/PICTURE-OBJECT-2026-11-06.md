# Live UserPreferences/NovoPictureSettings picture object — 2026-11-06

TV was off limits. This executes the shipped `aurora.elf` picture accessors
offline under `qemu-arm-static` and compares them with
`common/picture_config.c` on the shipped settingsDB blob. No TV, vendor
process, fbdev or `/dev/ntkdvif` was touched.

Program: `aurora.elf` MD5 `64c252340d3af6c5d66072611b7bad7a`.

## Method

`aurora.elf` is an ARM ET_EXEC linked at 0x10000, and its functions are
Thumb-2 (indirect calls need bit 0 set). The harness
`openaurora/tests/picture_object_vendor.c`:

- maps the three PT_LOAD segments at their link addresses (page-aligned,
  file prefix plus anonymous BSS);
- patches the `memcpy@plt` GOT slot at 0x17558c8 to the harness `memcpy`;
- writes 0 to the cached `Profile_IsHDRPictureModesEnabled` global at
  0x178c950, so the non-HDR energy branch is executed;
- builds a synthetic `UserPreferences`: 20×0x240 config bytes at +0x43f0
  from `Settings.m_picture_setting_configuration`, `m_last_zoom_mode` at
  +0x108, and a non-NULL `ProfileConfiguration*` at +0x7c38;
- runs the calls on a 2 MB thread stack (the accessors probe 0x3000 bytes
  below SP).

`common/picture_config.c` is the replacement model linked into the same
harness. It is the code `mod_settings` publishes.

## Executed vendor functions

All called through their Thumb entry (`symbol | 1`):

| address | function | coverage |
|---|---|---|
| 0x00f7e1b0 | `UserPreferences::getPictureSettingConfiguration(int)` | all 20 sources, pointer and 0x240 bytes |
| 0x00f7e1ec | `UserPreferences::setPictureSettingConfiguration(cfg,int)` | index 0 only (no twin lookup) |
| 0x00f7e45c | `UserPreferences::getPictureMode(int)` | all 20 sources |
| 0x00f7e498 | `UserPreferences::setPictureMode(NovoPictureMode,int)` | all 20 sources |
| 0x00f7e8e4 | `UserPreferences::getEnergySavingMode(int,NovoPictureMode,NovoDevice3DMode)` | non-HDR; 20 sources × 5 modes × 2 3D |
| 0x00f7e9e4 | `UserPreferences::setEnergySavingMode(int,NovoPictureMode,NovoDevice3DMode,NovoEnergySavingMode)` | non-HDR; 20 sources × 5 modes |
| 0x00f7bf50 / 0x00f7bf84 | `getDefaultZoomMode` / `setDefaultZoomMode` | read and round-trip |
| 0x00fb64b4 | `NovoPictureSettings::getZoomMode(int,NovoVideoContentFormat)` | format 1/2, and format 0 with a fake `isFullModeApplicable` (vtable slot 0x4c) |

The format-0 stub only supplies the environment decision; the field
selection is the shipped code. `NovoPictureSettings::setZoomMode` was not
executed: it also calls `EASITVControlInterface` reset/override/set methods
and the vtable `isFullModeApplicable`, which need the live engine object. Its
persistence calls (`get/setPictureSettingConfiguration`, `get/setZoomMode`
field choice) are executed instead.

`setPictureSettingConfiguration` at index 0 skips `getTwinSourceIndex`;
index != 0 dereferences `EASIControlInterface`/`EASITVControlInterface` and
the ICE descriptor table, so the twin-source write is decoded but not
executed. The code copies the 0x240-byte block to the selected slot and, when
`getTwinSourceIndex` returns a valid index, copies the same block there too.

## Verified result

```text
picture object: 20 source configs, 80 zoom formats, 200 energy cases, 242 setters
ok picture object: vendor UserPreferences/NovoPictureSettings accessors match the model
```

The 20 configs are byte-compared; mode, energy and zoom are compared field by
field; setters are applied to the vendor object and the same operation to the
model; the model reaches the same bytes.

## Value IDs, not array positions

The `SettingValue` construction order in the shipped sub-launcher branch is
not the value id order. The selection box stores the `SettingValue` id, and
that is what `setPictureMode`/`setZoomMode`/`setEnergySavingMode` persist.
Recovered IDs (from the decompiled `SettingValue` constructors; see
`artifacts/analysis/sub-launcher-menu-2026-11-06/menu-tree.json`):

| list | ids in list order |
|---|---|
| MODE | DYNAMIC 0, NATURAL 1, CINEMA 2, GAME 3, SPORT 4 |
| ENERGY_SAVING | OFF 0, MINIMUM_STR 1, MEDIUM 2, MAXIMUM_STR 3, AUTO 4, SCREEN_OFF 6, CUSTOM 5 |
| PICTURE_ZOOM | AUTO 1, 16:9 5, SUBTITLE 6, 14:9 3, 14:9_ZOOM 4, 4:3 2, 21:9 11, CINEMA 8, FULL 9 |

The old model stored the JSON array position. That was wrong:

- source 1's raw camera config has zoom field **1 = AUTO**, not 16:9;
- the dump's `m_last_zoom_mode` is **5 = 16:9**;
- configs **3, 4 and 14** have mode 1, zoom 5 and energy 3, i.e. the p0/03
  NATURAL / 16:9 / MAXIMUM capture;
- the dump's `m_current_source=5` config has mode 0, zoom 8-bit field 1
  (AUTO), full field 9 (FULL) and energy 0 (OFF).

`openaurora/menus/sub_launcher.json` now carries the ids; `ui_vendor` stores
and reads the id while keeping the JSON index for cycling/rendering.

## Format-0 zoom

`NovoPictureSettings::getZoomMode(index, format)` was decoded instruction by
instruction (0x00fb64b4):

```text
format 2 -> config+0x0c
format 1 -> config+0x08
format 0 -> isFullModeApplicable(index) ? config+0x0c : config+0x08
```

`setZoomMode` (0x00fb6a0c) writes `+0x0c` for format 2, `+0x08` for format 1,
and for format 0 writes `+0x0c` when full mode is applicable **or** the value
is `ZOOM_MODE_FULL` (9), otherwise `+0x08`. `mod_settings` implements this
with `picture.full_mode` (default 1, the captured HDMI1 1080p case) and the
FULL special case. The getter still reads `+0x08` after a FULL write when
full mode is not applicable; that vendor asymmetry is preserved.

## Reproduction

```sh
make -C openaurora check-picture-object
# or
openaurora/tools/qemu-picture-object-check.sh
```

The script extracts the blob and `m_last_zoom_mode` from the shipped
settingsDB with host sqlite3, builds the ARM harness with the target
toolchain, and runs it under qemu-arm-static. It requires the shipped ELF and
settingsDB; a missing input is an error, not a skip.

## Limits

- The HDR energy variants (`+0x446c`, `+0xa10`, `+0xa48`) are not executed;
  the harness forces `Profile_IsHDRPictureModesEnabled()==0`. The non-HDR
  branch is verified.
- `NovoPictureSettings::setZoomMode` and `getPictureSettingsIndexForCurrentSource`
  are decoded but not executed (they need the live EASI/InputManager graph).
- The twin-source config copy is decoded but only index 0 (no twin) is
  executed.
- This proves the persisted field path, not the live vendor object in a
  running Aurora process, and not PQ/video application.
- The TV database write still requires `settings.readonly=0`.
