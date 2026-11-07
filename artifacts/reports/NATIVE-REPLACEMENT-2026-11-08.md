# Native replacement composition — 2026-11-08

Status: the OmarchyTV-native shell is now the **autoloaded default** in a
separate replacement bundle and was visible on the TV in a real-platform
takeover. This is an implementation/visibility milestone, not UI parity or
hardware acceptance.

## Composition

`make TARGET=tv BUILD=build-tv-v3 native-replacement-bundle` builds the same 33
modules and binaries as the full replacement, but copies
`tools/boot/openaurora-native.conf`. The autoload list is:

```text
plat,display,panel,ir,audio,source,power,tuner,settings,service_db,
image_decode,epg,pvr,channels,i18n,audio_curve,audio_mixer,app_launch,
audio_amp_profile,audio_amp,cec,ticker,ui_clock,ui_anim,ui_engine,
ui_omarchy,lua
```

The vendor portal stack (`ui_vendor`, `ui_theme`, `ui_profile`, `ui_menu`,
`ui_list`, `ui_navigation`) is not autoloaded. Those modules remain in the
bundle for explicit diagnostics/reload, but none is needed by the native shell.
`ui_engine` requires `display`; `ui_omarchy` requires `ui.engine` and looks up
`launcher.db`, `settings`, `source`, `sched` and optional `ui.lua`.

## TV takeover evidence

Bundle: `/buffer/oa-native-20261108`, tar sha256
`a0174cba3f0c72e8b1bbb841782baf368f00e7a9537c0d8ef6735a31cfd7c726`.
Preflight passed (`--check`, no module/hardware access).

Takeover module list included:

```text
ui_engine      started   id=31  ./modules/mod_ui_engine.so
ui_omarchy     started   id=35  ./modules/mod_ui_omarchy.so
lua            started   id=40  ./modules/mod_lua.so
display 1920x1080 32bpp 2 frames
no such module: ui_vendor
```

`/tmp/oa-full.log` recorded:

```text
ui_engine    service up: ui.engine v2
ui_omarchy   present: scene=44ms mirror=0ms sync=0ms pan=8ms rect=0,0 1920x1080
oa-supervise: host loop alive, first present acknowledged
openaurora: 26 module(s) loaded, control on /tmp/openaurora.sock
```

So the native shell painted two frames, owned the visible OSD path over the
real platform, and the vendor portal never started. This supersedes the earlier
note that `ui_omarchy` was excluded because `ui_vendor` owned the single
`ui.engine` skin.

## App-launch consumer

`mod_app_launch` consumes `OA_TOPIC_ACTION` `SELECT`, maps the launcher display
name through `oa_apps.map` to a stock NAM application name, and spawns the
one-shot `oa-app-launch` helper. The helper calls the recovered stock contract:

```text
ice_APM_StartProcess(name, args)
  -> LIBNAM_APP_SendCommand(0x106,
       { char name[40]; char args[8192]; int state=2; })
registration: LIBNAM_APP_Register("COM.NOVATEK.IDTV", 0x007A0035, cb1, cb2)
```

The libnam decompilation confirms `LIBNAM_APP_SendCommand` does **not** create a
client for command `0x106`; registration must precede it. The helper's callbacks
are no-op stubs for this short-lived launcher registration; it does not claim to
be the full idtv app-server event implementation.

The consumer is host-tested: mapped spawn, explicit argument override,
unknown-name rejection, disabled-mode rejection and reap. The native config
autoloads it with `app_launch.enable = 0` because the active
`nvt.app.debug.ini` profile disables Netflix/Amazon/YouTube and the map is
per-profile. No app was launched on the TV. Bundle `oa-native-20261108b.tgz`
sha256 `6ce14ca906b07bd5bf1d578655235bd33880a9108b537195fb7eb49f732623ac`;
`oa-app-launch` links `libnam.so` with RPATH `/vendor/lib/nlib`.

## Restore and audio

`takeover-full.sh restore` returned stock (`aurora.elf 4468`) and reported
`optical control restore -> ok` / `opu volume restore -> ok`. The known hidden
audio-DSP state still required a reboot: after `reboot -f`, stock came up as
appmgr 1580 / aurora 1781 and the critical controls were healthy. This is the
documented ALSA-CONTROL limitation, not a new regression.

## Limits

- `ui_omarchy` is still the shell skeleton: home/sources/apps/settings/about,
  focus navigation, clock, launcher list and emitted actions. It is not full
  vendor UI parity (animations, menu hierarchy, localized assets, channel UI).
- `OA_ACTION_SELECT` now has a host-tested consumer and libnam helper, but no
  TV launch was attempted. The active `nvt.app.debug.ini` profile disables the
  Netflix/Amazon/YouTube entries and `oa_apps.map` is not profile-verified; the
  helper's registration callbacks are no-op stubs.
- Takeover audio restore remains reboot-required for the hidden DSP state.
- Cold boot/replacement boot path, RF/DVB lock, CEC TX/RX, DVR, backlight
  feedback, IR repeat and amplifier mute remain unaccepted.
- Restore snapshot hash/state: source tree commit follows the report.

Report: NATIVE-REPLACEMENT-2026-11-08. Related: NATIVE-UI-2026-11-07,
REPLACEMENT-SYSTEM-2026-11-07, ALSA-CONTROL-2026-11-07, APP-MANAGER-CONFIG.
