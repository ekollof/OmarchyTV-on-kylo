# Firmware menu tree, JSON model and ui.menu service — 2026-11-06

This closes the “one menu leads to another” decoding gap for the portal
sub-launcher. The tree, geometry and 300 ms item animation come from the
shipped firmware and BinSVG resources; the runtime form is JSON and the
entries carry action strings. No TV button capture was needed for this step.

## Firmware tree

`LauncherDialog::showSubLauncher` (0x00ed9888) builds a `SubLauncherDialog`
for the focused launcher item. It is reached from:

- `LauncherDialog::onSubLauncherTimer` after the 750 ms
  `setSubLauncherOpenTimer` (`setTimer(...,0x2ee,6)`), and
- `LauncherDialog::onUpDownPress` (0x00eddb08) on the down key.

`SubLauncherDialog::initialiseFields` (0x00ee3614) dispatches on the launcher
item type at `+0x4b8`:

| type | function | decoded branch |
|---|---|---|
| 2 | `initialisePictureSettingsShortcuts` | picture |
| 3 | `initialiseSoundSettingsShortcuts` | sound |
| 7 | `initialiseSystemSettingsShortcuts` | system |
| 0 | `initialiseChannelListShortcuts` | channel list |
| 52 | `initialiseFVPShortcuts` | FVP (excluded) |
| 61 | `initialiseAIMenuShortcuts` | AI menu |
| other | recommendations / advertisement | excluded |

The branch items are `SettingItem` constructors; the value lists are
`SettingValue` constructors. The decoder resolves the constructor arguments
(`DAT_*` string pointers) from the ELF:

```
branch 2 picture
  item 0 MODE -> picture_modes
  item 1 ENERGY_SAVING -> OFF, MINIMUM_STR, MEDIUM, MAXIMUM_STR, AUTO,
                          SCREEN_OFF, CUSTOM
  item 2 PICTURE_ZOOM -> ZOOM_MODE_AUTO, ZOOM_MODE_16_9, ZOOM_MODE_SUBTITLE,
                         ZOOM_MODE_14_9, ZOOM_MODE_14_9_ZOOM, ZOOM_MODE_4_3,
                         21:9, ZOOM_MODE_CINEMA, ZOOM_MODE_FULL
branch 3 sound
  item 0 EQUALIZER -> MUSIC, MOVIE, SPEECH, FLAT_EQ, CLASSIC, USER
  item 1 CEC_SPEAKERS -> DISPLAY, TV, CEC_AMPLIFIER
  item 2 DOLBY_AUDIO_OR_MODE -> OFF, SMART, MOVIE, MUSIC, GENRE_CODE_NEWS
  item 3 SOUND_OUTPUT -> DISPLAY, TV, SPDIF, HDMI_ARC, HEADPHONE_ONLY, LINEOUT
  item 4 AVL -> OFF, NORMAL, NIGHT, AUTO
  item 5 AVL -> OFF, ON
branch 7 system
  item 0 SLEEP_TIMER -> sleep_timer_values
  item 1 AUTO_TV_OFF -> auto_tv_off_values
  item 2 SUBTITLE -> OFF, ON
  item 3 AUDIO_LANG -> languages
branch 0 channel_list
  item 0 FILTER_FAVOURITES_LABEL -> favourite_lists
branch 61 ai
  item 0 AI_TXT -> OFF, ON
branch 52 fvp (excluded)
branch default recommendations (excluded)
branch default advertisement (excluded)
```

The sound item 2 key is selected at runtime from `DAT_00ee1de0`
(“Dolby Audio”) or `DAT_00ee1de4` (“MODE”); system item 0 from
`DAT_00ee239c` (`SLEEP_TIMER_STR`) or `DAT_00ee2398` (`OFF_TIMER`); system
item 1 from `AUTO_TV_OFF`/`AUTO_TV_OFF_NO_TUNER`. The JSON marks those items
with a `dynamic` source note instead of pretending the placeholder is a
firmware key.

## Geometry and animation

`DialogSubLauncher.svg` (resource 189) contains:

```
background      0,0 1920x1080
main_rectangle  0,0 1920x1080
listBox         189,255 1774x222
```

`SettingItemsListBox` uses the dialog’s named `listBox` rect; the
`SettingSubLauncherListBoxFA.svg` / `SettingItemsListBoxRowItems_sublauncher`
/ `sublauncheritemFA.svg` resources provide the row and item skins:

```
item scalerect  121,251 240x140
item glyph      121,253 240x138
item textPos    79,374 323x39
```

`sublauncheritemFA.svg` animation records (all 300 ms, linear, begin 0):

```
#scalerect transform scale  1 1 -> 1.35 1.35
#glyph     x                 0 -> -42
#glyph     y                 0 -> -24
```

`SettingSubLauncherListBoxFA.svg` has the companion
`#selectionIndicator y 0 -> 100` over 300 ms. The scaled rect is anchored at
the button window top-left and uses the named `#scalerect` size, as recorded
in SUBLAUNCHER-GRID-ANIM.

## JSON runtime model

`openaurora/menus/sub_launcher.json` is the runtime form. Each dialog has
`list`, `item` geometry and `branches`; each branch has `launcher_type`,
`name` and `items`; each item has `id`, `key`, an optional `action`, an
optional `dynamic` source note and `values`. Values are either a dynamic
marker string or an array of `{key, action}` entries.

```json
{
  "launcher_type": 2,
  "name": "picture",
  "items": [
    {
      "id": 1,
      "key": "ENERGY_SAVING",
      "action": "setting:ENERGY_SAVING",
      "values": [
        { "key": "OFF", "action": "set:ENERGY_SAVING=OFF" }
      ]
    }
  ]
}
```

Adding a branch or entry is a JSON edit. The action string is opaque to the
loader and is dispatched by the consumer (`oa_menu_dispatch`), which is the
hook for opening a dialog, selecting a screen, or calling a module function.
The JSON file is generated from the firmware evidence by
`tools/re/menu-tree.py --emit-json`; manual additions are allowed and are not
overwritten unless the decoder is rerun.

## Implementation

- `tools/re/menu-tree.py`: bounded BinSVG reader + Ghidra-decompile parser;
  resolves `DAT_*` strings, emits `menu-tree.json/txt` and the runtime JSON.
- `common/json.c` / `include/openaurora/json.h`: minimal bounded JSON DOM.
- `common/menu_model.c` / `include/openaurora/menu_model.h`: menu document
  loader, branch lookup, action dispatch and item animation.
- `modules/ui_menu/ui_menu.c`: reloadable `ui.menu` v1 provider. It loads
  `ui_menu.file` (default `/buffer/openaurora/menus/sub_launcher.json`) at
  start and publishes `document`/`dialog`/`branch`; reload replaces the whole
  document.
- `include/openaurora/services.h`: `OA_SVC_UI_MENU` v1 for module consumers.
- `menus/sub_launcher.json`: generated runtime tree.

## Verification

```
ok menu model: JSON firmware tree, actions and 300 ms item animation
ok ui.menu service: JSON tree, reload, and failed-load isolation
full_check=0
```

The menu test loads the JSON, asserts the five live branches, the picture
values and their `set:` actions, the dynamic sound/system notes, the 300 ms
animation at 0/0.5/1, then loads a second JSON with a new branch and entry and
dispatches its `open:new_dialog` action. The service test loads/unloads the
module, queries the tree, proves a bad path publishes no stale service, and
recovers.

The ARM module builds in `build-tv-v3`:

```
mod_ui_menu.so  GLIBC_2.4
```

## Limits

- `ui_vendor` does not consume the JSON tree yet. It still paints its captured
  settings states; routing `open:`/`setting:`/`set:` actions to screens and
  settings is the next integration step.
- `SVGListBoxBase`’s horizontal row origin fields (`+0x204`, `+0x208`,
  `+0x210`) are not fully traced. The listBox rect and 240x140 item size are
  proved; the exact first-row offset is not.
- Dynamic keys are marked, not guessed; the runtime branch depends on the
  selected launcher item and model/region conditions.
- FVP, recommendations and advertising branches are deliberately excluded.
- No on-TV run of `mod_ui_menu`; host and ARM build evidence only.
