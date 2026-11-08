# Native OmarchyTV shell + Lua panes — 2026-11-08

Status: **host-tested native UI milestone; no TV acceptance yet.**

Reference: <https://gitlab.com/piermark/omarchytv>, commit
`8489a193cb4b2ada7c40eaf7ffe2f8c73074e878` (2026-11-08). The checked-in
`launcher/omarchy/` snapshot is older; use the GitLab tree for current
OmarchyTV behaviour. The production UI stays native (`ui_engine` canvas): no
browser, webview, HTML/CSS/JS runtime or web app is started.

## What changed

`mod_ui_omarchy` is now a native workspace shell modelled on the reference,
replacing the four-tile placeholder:

- 52 px top bar: Omarchy mark, five numbered workspaces with the active
  underline, centred `EEE dd MMM  hh:mm A` clock, right status modules.
- Workspace 1: status terminal (host/shell/source/volume), three widget panes
  and a six-slot dock. The dock is `TV` plus the first launcher apps, with
  `Sources`/`Apps`/`Settings`/`About` filling unused slots when the launcher
  is empty. Dock title/hint follow focus.
- Workspace 2: four-column input grid (TV, BACK AV, HDMI 1-3, YPbPr,
  VGA/PC, Wireless Display, AV Share).
- Workspace 3: seven-column app grid from `launcher.db` (up to 21 entries).
- Workspace 4: theme, CEC, source-switch and About rows. Theme changes are
  local; CEC still writes `m_cec_enabled` through the settings service.
- Workspace 5: native-shell/about panel.
- Keys: RC 20/19/21/22 arrows, 53 Select, 10 Exit, 37 Exit2, 48 Menu,
  1-5 workspace digits, 32/33 previous/next workspace. Up enters the bar;
  bar Select switches workspace. Internal back does not emit a spurious
  `OA_ACTION_BACK`; Exit on the dock TV tile does.
- Six palettes from the reference `data.js` (`last-horizon` default,
  `tokyo-night`, `catppuccin`, `gruvbox`, `nord`, `everforest`).
- OAI1 wallpaper support through `ui.omarchy.wallpaper`; missing artwork
  falls back to the theme gradient. `tools/ui/mk-wallpaper.py` converts a
  still to OAI1. No wallpaper artwork is committed (the reference's
  `last-horizon` image is used there by permission, not MIT).

## Lua widget panes (ui.lua v2)

`ui.lua` now exposes a bounded pane contract in addition to the existing
theme/layout/key/fetch/timer/state hooks:

```lua
oa.pane{ id="weather", title="weather", lines={
  {"Alessandria", "9 C", "up"},
  {label="wind", value="NW 8 km/h"},
  "clear",
} }
```

Limits are enforced before copying: 3 panes, 8 lines, title 23 bytes,
label 15, value 63, id 31. A line is a string or
`{label=..., value=..., tone="up"|"down"}`. Updating an existing `id`
replaces it. The service copies the panes; the skin receives no
`lua_State`.

The skin renders up to three panes in the home widget row after running the
`paint` hook; native status boxes fill unused slots. `oa.pane_clear()`
removes all panes. `OA_LUA_API_VERSION` is now 2; old consumers requesting 1
still resolve.

The default script `lua/omarchy.lua` publishes a `system` pane
(`/proc/loadavg`, memory, uptime) and a `clock` pane, and contains a
commented `oa.fetch` weather/RSS template. It is editable; `ui.lua.script`
selects the file and `ui.lua.data` is the sandbox write root
(`/buffer/openaurora/lua-data`). The native autoload order now starts `lua`
before `ui_engine`/`ui_omarchy`; the bundle ships `lua/omarchy.lua`.

## Takeover settings restore

New host-tested control commands `settings-get <column>` and
`settings-set <column> <int>` let `takeover-full.sh` snapshot `m_volume`,
`m_current_source` and `m_cec_enabled` after the core starts, and restore them
before `quit`. It also copies `settingsDB.db` to
`/buffer/oa-settings-before/` for manual recovery. This addresses the
bundle-e regression where the test left stock source/volume values behind.
The restore has not been exercised on the TV; the hidden DSP/route state may
still require a reboot.

## Evidence

Host-only; no TV, HAL, framebuffer or vendor library was touched.

```text
make check-ui-skin            # new shell geometry, dock/apps/sources/settings,
                              # wallpaper blit, focus, OSD, skin reload
make check-ui-lua             # sandbox, theme/layout, oa.pane contract,
                              # fetch/timers/spawn/quiesce
make check-ui-omarchy-quiesce # ABI 3 handoff/unregister
make check-ui-skin-socket     # real binary socket/switch/fallback
```

All pass. A local capture run with the vendor EF41 pack
(`OA_UI_FONT_INDEX`/`OA_UI_FONT_PACK`) produced `home/sources/apps/settings/
about` 1920x1080 RGBA snapshots; the layout, dock, bar, clock and text were
visually checked. A Lua-pane variant (`stats`, `local widgets`, `temp 9 C`,
tone up) was also captured and checked. The test wallpaper is a 2x2
checkerboard; the real wallpaper is an OAI1 import.

Reference layout facts used: five workspaces and a centred clock, six-slot
dock with TV first, four-column TV grid, seven-column app grid, and the
`last-horizon`/`tokyo-night` palettes (`webos/app/index.html`, `data.js`,
`app.js` at the pinned commit).

## Limits

- No TV acceptance: no scanout, input or visible-layout check.
- The EF41 bitmap font is still used; the reference's JetBrains/Caskaydia
  Mono TTF is not converted to the native pack.
- App tiles are text labels; launcher icons and the image-decode path are
  not wired into the native grid yet.
- No focus animation, scale/outline motion, animated wallpaper, screensaver,
  arcade/plugins/Cinemarchy/Grok companion, or theme/wallpaper persistence.
- `source.enable` remains disabled. The VIN-only path is still not the
  firmware `plat_vin_slot_connect`/VPLT/V4L2/DRM route.
- Mute remains the stock external-output no-op; no volume-0 substitute.
- The Lua panes are host-tested for data and bounds, not for a live weather/
  RSS feed or a TV-rendered pane.
