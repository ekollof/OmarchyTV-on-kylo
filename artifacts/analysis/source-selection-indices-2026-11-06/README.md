# Bronlijst row -> picture source index — 2026-11-06

TV-offline Ghidra/ELF evidence for mapping the fixed Bronlijst rows to the
firmware picture-configuration index. Program: `aurora.elf` MD5
`64c252340d3af6c5d66072611b7bad7a`.

| file | scope |
|---|---|
| `decompile.txt` | `NovoExternalAVSourceNavigator::get*Index` wrappers |
| `input-getters.txt` | `InputManager::getExt1/2/3/4`, `getCVBS`, `getWiDi` group ids |
| `group-consts.txt` | `InputManager::getFirstHDMI` (3), `getYPBPR` (5), `getPC` (6), `getAPM` (16) |
| `group-map.txt` | `InputManager::getInterfaceByGroupID`: scans ICE interfaces by `getVideoGroupIDOfExtInterface` |

## ICE descriptor table

`ICE_ExternalNumberOfInterfaces` returns 13 and `ICE_ExternalGetName` indexes
`descriptor + i*0x38`; the name is inline at +0x2c (four chars) and the video
group id is at +0x28. The file table at 0x0175de8c:

| index | name | group |
|---|---|---|
| 0 | EXT1 | 1 |
| 1 | EXT1 | 2 |
| 2 | Fav (CVBS/BACK AV) | 8 |
| 3 | Svhs | 7 |
| 4 | HDMI | 3 |
| 5 | HDMI | 4 |
| 6 | HDMI | 9 |
| 7 | HDMI | 12 |
| 8 | YPbPr | 5 |
| 9 | VGA | 6 |
| 10 | DVD | 14 |
| 11 | Widi | 15 |
| 12 | APM | 16 |

`getFirstHDMIIndex` is group3 → index4; `getCVBSIndex` group8 → index2;
`getYPBPRIndex` group5 → index8; `getVgaIndex` group6 → index9;
`getWiDiIndex` group15 → index11; `getApmIndex` group16 → index12.

## Picture index

`InputManager::setCurrentExtSrc(index, main)` stores `index` and calls
`UserPreferences::setCurrentSource(index+1)`.
`NovoPictureSettings::getPictureSettingsIndexForCurrentSource` returns
`getCurrentExtSrc(true)+1` (0 for the tuner). So the picture-configuration
index is the descriptor index + 1. The Bronlijst rows map to:

| row | source | descriptor | m_current_source |
|---|---|---|---|
| 0 | TV | tuner | 0 |
| 1 | BACK AV | 2 | 3 |
| 2 | HDMI1 | 4 | 5 |
| 3 | HDMI2 | 5 | 6 |
| 4 | HDMI3 | 6 | 7 |
| 5 | YPbPr | 8 | 9 |
| 6 | VGA/PC | 9 | 10 |
| 7 | Wireless Display | 11 | 12 |
| 8 | AV share/APM | 12 | 13 |

The dump's `m_current_source=5` is therefore HDMI1, consistent with the Steam
Deck source and `ui_vendor`'s `SRC_PICTURE_INDEX`.
