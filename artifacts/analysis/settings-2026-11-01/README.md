# Captured picture-settings (Beeld) state on fb1 (2026-11-01)

Purpose: put the p0/03 "Beeld" state on the real set as the first settings
screen, byte-exactly, while the firmware interaction path is still undecoded.
It is a captured state, not reconstructed settings widgets.

## What the screen is

p0/03 is the `SubLauncherDialog` picture-settings overlay:
`SubLauncherDialog::initialiseFields` (0xee3615) dispatches item type 2 to
`initialisePictureSettingsShortcuts` (0xee0ed0). The surrounding resources are
`DialogSubLauncher` (189), `DialogSettings` (175),
`DialogSettings_fullscreen` (177), `DialogSettings_tab` (179),
`DialogTabMenu` (191), `SettingHorizontalListBox*` (467/471) and
`SettingListBox*` (475/479); see MEMORY SETTINGS-SCREEN-2026-11-01.

The screen ships three R/B-corrected bands from the capture:

| blob | source rect | size |
|---|---|---|
| `settings_rail` | 0,180 | 176x660 (through the fifth rail tile's bottom edge) |
| `settings_top` | 0,240 | 1920x180 (Modus / Energiebesparing / Beeld zoomen tiles) |
| `settings_cat` | 0,440 | 1920x260 (category tiles, labels and the focused "Beeld" caption) |

The remainder is the corrected background `(13,14,14)`. p0/03 stores it as
`(14,14,13)`, i.e. the same R/B swap the resource palette shows on the panel
and rail colours. This was found by comparing the live capture: the bands were
exact but the background was one channel pair off and the "Beeld" caption plus
the rail bottom fell outside the first band set.

Extraction: `openaurora/tools/extract-settings-bands.py CAPTURE OUTDIR`.
The PNG saved from tvshot and the swap-corrected vendor capture now hash
identically:

```
sha256 1312575760d88ed205c09a1e72f58e04f926e6f256b3d764e3d73366e65a5675
```

## Run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 26424, staged in `/tmp/oa-src-stage` |
| modules | plat stub, display, ui_engine, ui_vendor |
| mod_ui_vendor md5 | 524d58bfc4891b74309e96b57c074a23 |
| config | `autoload=plat,display,ui_engine,ui_vendor`, `ui.screen=settings`, `ui.dir=/tmp/oa-src-stage/ui`, `ui.fps=0` |
| fb1 backup | `fb-restore save`: 1920x1080 virt 1920x2160 32bpp, yoffset 1080, 16,588,800 bytes |
| capture | `/dev/fb1` frame 1, `tools/tvshot.py --layer 1` |
| result | 0 differing pixels of 2,073,600; max channel delta 0 vs the corrected p0/03 |
| restore | `fb-restore restore`: bytes 16588800, offset 0,1080; staging removed; no `openaurora` left |

A first run (PID 25849, module 72cfe0bd...) exposed the two gaps above; its
fb1 backup was restored before restaging, so the second run's backup is stock
pixels again.

## Not verified / not claimed

- This is a **static captured state**, not the settings screen: no navigation,
  no per-category item lists/values, no localized labels from firmware.
- Focus/selection animation, list scrolling and `SettingsDialog` popup
  placement remain undecoded and unimplemented.
- fb1 readback is not visible scanout proof.
- The 1-pixel background choice comes from consistently applying the
  documented capture correction; no firmware resource was decoded for it.
