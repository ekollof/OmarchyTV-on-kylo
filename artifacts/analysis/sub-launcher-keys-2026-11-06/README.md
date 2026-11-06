# Sub-launcher key flow — 2026-11-06

TV-offline Ghidra evidence for which keys move the card focus and which change a
value. Program: `aurora.elf` MD5 `64c252340d3af6c5d66072611b7bad7a`.

| file | scope |
|---|---|
| `listbox-key-decompile.txt` | `SVGListBoxBase::onKeyPress` (0x010bcf4c) |
| `listbox-move-decompile.txt` | `SVGListBoxBase::onUpKey`/`onDownKey`/`onSelectKey` (0x010bc964/0x010bc9f8/0x010bcbe4) |
| `value-flow-decompile.txt` | `SettingItemsListBox::onRightLeftPress` (0x00d508b8), `SettingItemsListBoxRow::onRightLeftPress`/`onSelectPress`/`getSelectedItemValue` |
| `keys-decompile.txt` | `SubLauncherDialog::handleKeyPressMessage`/`onUpDownPress`/`onGuidePress`/`setSelectedItem`/`isItemSelected` (mostly inlined; no extra key logic) |
| `picture-update-decompile.txt` | `updatePictureMode`/`updateEnergySaving`/`updateZoomMode`/`initialisePictureSettingsShortcuts`; the setting backend is reached through an abstract vtable at `+0x318`, no column names in this layer |

Verified key flow, joining `UI-MENU-FLOW-2026-11-04` ICE ids:

- ICE Up 0x20a / Down 0x20b → `SVGListBoxBase::onUpKey`/`onDownKey` → selection
  move + `startSelectAnimation` (the 300 ms selection animation).
- ICE Left 0x20c / Right 0x20d → `SettingItemsListBox::onRightLeftPress` →
  the selected row's `SVGSelectionBox`/checkbox/text-entry/slider. For the
  picture shortcut row this is a value change, not a focus move.
- ICE Select 0x20e → `onSelectKey`; `SettingItemsListBoxRow::onSelectPress`
  forwards only for type 0x16 (checkbox), so a selection-box card has no
  select action.
- `getSelectedItemValue` returns the selection box's selected item id for row
  types 2/3. The sub-launcher card row therefore changes values in place with
  Left/Right.

`picture-update-decompile.txt` shows the update calls per setting but not the
settingsDB column names; live value acquisition remains open.
