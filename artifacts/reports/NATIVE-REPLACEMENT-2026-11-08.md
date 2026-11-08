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

### TV app-launch attempts (runtime swaps blocked)

The owner approved a temporary retail appmgr. A second appmgr with a custom
retail list (`idtv=n`, `dialserver=n`, `youtube_test=n`; Netflix/Amazon/YouTube
enabled) initialized DirectFB but failed at `_MGR_InitSaWMan` CreateManager
(1278491788) because the stock appmgr owned SaWMan. Stopping the stock appmgr
dropped SSH and the `ash ./run.sh` supervisor rebooted the set; stock returned
as appmgr 1581 / aurora 1789. The debug console `@netflix` wrote but did not
launch (disabled profile).

A second runtime attempt paused the `run.sh` supervisor, disabled the
watchdog, stopped the stock appmgr, and started the custom retail appmgr. It
created SaWMan (`DirectFB/Main: Initialization End`) and started privserver,
but never created the NAM manager socket
`/tmp/.NPC.COM.NOVATEK.COMP.NAM`. `oa-app-launch` registration failed with
`connect ... Connection refused`; no Netflix started. The first helper run
also exposed a real link bug: `libnpc.so` needs `sem_init` from libpthread.
The helper now links libpthread (NEEDED `libpthread.so.0`, sha256
`c6a99a451fd525ade4f5e8f61f311404b2d805f2bc93d27eef8a91e29323a070`).
A reboot restored stock appmgr 1578 / aurora 1780; staging removed and controls
healthy.

Runtime appmgr swaps are therefore not a reliable acceptance path. TV
app-launch acceptance needs a boot-time app list/retail profile (boot hook) or
a persistent profile change; both are owner gates. The TV is available again;
no further appmgr swap is planned without that gate.

## Native UI target

`ui_omarchy` is a placeholder. The design/behavior target is the browser
OmarchyTV port in `launcher/omarchy/` (P3, upstream webOS app): `index.html`
(23 KB), `app.js` (131 KB), `data.js`, `dhh.js`, `compat.js`,
`omarchy_themes.json`, fonts and wallpapers. It was verified on this TV with the
top bar/workspaces 1-5/clock, OMARCHY TV logo panel, weather/system panels, app
grid, 28 themes, wallpapers and the Aquarium screensaver. The final native UI
should reproduce that look/work with the native renderer (`ui_engine`/canvas),
reusing the theme/wallpaper/font assets. The production UI is native-only: do
not embed, launch or webview the browser/webOS app; the browser is the
visual/behavioral reference. Theme, wallpaper and font data may be imported as
native assets, not executed as a web app. Do not treat the current placeholder
screens, 4-tile home or opacity as parity, and do not polish them as the target.

## Source test and stock state 2026-11-08

Bundle e set `source.enable=1`; the source module opened the VIN (fd 22) and the
UI wrote `m_current_source`, but the displayed source did not change. The VIN
`Hal_Vin_SelectInput`/`SetStreamStart` path alone is not wired to the display;
the companion `plat_vin_slot_connect`/VPLT/`Hal_Video_Set_V4l2Info`/audio path is
still missing. The placeholder source transition and `source.enable=1` were
reverted; do not enable source switching until the full firmware path is
recovered. After the test the stock settings DB had `m_current_source=5` (HDMI1) and
`m_volume=0`; the ALSA route controls were healthy but the soundbar stayed
silent and the owner had to force-reboot to restore audio. This is a
stock-settings/restore regression, not just the DSP. The takeover restore must
snapshot/restore the stock volume/source settings or the test must not write
them; no further TV tests until that is fixed.

## Native volume/mute OSD

`ui_omarchy` now consumes the remote volume keys globally: code 16 up, 17 down,
13 mute (IR-KEYMAP codes). It looks up `audio`, calls `set_volume`/`set_mute`,
and draws a 1.5 s bottom-right overlay with a 0..100 bar. If the audio service
is absent or refuses (disabled/CEC policy), the key is still consumed so it
cannot move the current screen selection. The host `check-ui-skin` test verifies
the service calls, the OSD fill and the timer hide:

```text
ok   volume/mute keys call the audio service and draw the native OSD
```

TV test 2026-11-08 (bundle d, native UI 1920x1080, 27 modules):
- Volume up/down: the bottom-right OSD appeared and the soundbar volume
  changed. This is TV-accepted for the native volume path.
- Mute: IR code 13 reached the UI, but `audio.set_mute` returned -1 and the
  soundbar did not mute.
- Sources: up/down was reversed, OK did not switch, Exit did nothing, Back
  returned.

Firmware mapping (IR-KEYMAP + SETTINGS-INPUT/TAB-DISPATCH/TAB-LEAF):
RC 20=KeyUp/ICE 0x20a, 19=KeyDown/0x20b, 53=KeySelect/0x20e,
10=KeyExit/0x210, 37=KeyExit2/0x25b, 48=KeyMenu/0x22d,
13=KeyMute/0x222, 16/17=VolumeUp/Down/0x220/0x221.
SettingsDialog/TabDialog route 0x20a/0x20b to onUpDownPress,
0x210/0x25b/0x22d to onMenuPress, and 0x222 directly to
DemoApp::toggleAndDisplayMuteState. The native shell now maps RC 20=Up,
19=Down and RC 37 to the back/menu action; focused `check-ui-skin` passes.
The source OK failure is not the key: the source module is gated by
`source.enable` and does not implement the stock
`plat_vin_slot_connect`/VPLT/`Hal_Video_Set_V4l2Info` route. The mute failure
is the audio interface, not the key. Source and mute recovery from the
firmware is in progress before either path is enabled.

Bundle `oa-native-20261108d.tgz` sha256
`daa8e0cf5798ae4f1f05dda6e6af00deb6b55c82ca3bf5464415dcaa3fbecb91`;
ARM `mod_ui_omarchy` sha256
`d32774e1ef22dcf75d173f118d653b095e3498f85095ee5a8b95260d2f227906`.
Stock restored after the test: appmgr 1563 / aurora 1781, `/conf/oa_safe`
present, audio controls healthy.

Mute follow-up: `DemoApp::toggleAndDisplayMuteState` at `0xcd82a8` first checks
`Profile_IsSoundOutputSelectionEnabled`/`isTestProductModeEnabled`, then the
audio object's output type (`vtable+0x154`) and another object's `+0x5c`.
For external output types 1/2 with the zero condition it jumps straight to the
function end; only the internal path toggles the audio object's mute and shows
the status icon or opens `VolumeDialog`. The offline settingsDB has
`m_sound_output_mode=1`, `m_digital_audio_out=1`, `m_spdif_out_on=1`,
`m_sound_mode_external=4`, consistent with the optical external output. The
owner's "mute does nothing" is therefore likely stock behavior for optical,
not a missing `PLAT_AOUT` call; do **not** substitute volume 0. The exact
`+0x5c` condition/class is still being recovered. The
`PLAT_AOUT_SetSystemMute`/`SetSpeakerHWMute` callers found so far are media,
audio-start and sound-output-mode paths, not the DemoApp key branch.

## Restore and audio

`takeover-full.sh restore` returned stock (`aurora.elf 4468`) and reported
`optical control restore -> ok` / `opu volume restore -> ok`. The known hidden
audio-DSP state still required a reboot: after `reboot -f`, stock came up as
appmgr 1580 / aurora 1781 and the critical controls were healthy. This is the
documented ALSA-CONTROL limitation, not a new regression.

## Limits

- `ui_omarchy` is a placeholder, not the target. The native UI should reproduce
  the browser OmarchyTV app in `launcher/omarchy/` (workspaces, top bar, clock,
  app grid, themes, wallpapers, screensaver) with the native renderer. Do not
  polish the placeholder's 4-tile home/opacity as parity.
- Source switching is not wired: the VIN select/start returns but does not
  change the display; the VPLT/`Hal_Video_Set_V4l2Info`/audio companion path is
  missing. `source.enable` is reverted to disabled. The stock settings DB is at
  `m_current_source=5`, `m_volume=0`; use the stock remote to return to TV and
  raise volume.
- `OA_ACTION_SELECT` has a host-tested consumer and libnam helper. The
  owner-approved temporary retail appmgr process swap failed: a second appmgr
  could not create the SaWMan manager, and stopping the stock appmgr triggered
  a supervised reboot (stock debug appmgr/aurora restored). The debug console
  `@netflix` wrote but launched nothing. TV app-launch acceptance needs a
  one-boot app-list override or boot/profile gate; `oa_apps.map` is not
  profile-verified and the helper callbacks are no-op stubs.
- Volume up/down and the native OSD are TV-accepted. Mute is likely stock
  no-op for the external optical output: `DemoApp::toggleAndDisplayMuteState`
  skips external output types 1/2, and the settingsDB has
  `m_sound_output_mode=1`, `m_digital_audio_out=1`, `m_spdif_out_on=1`. Do not
  substitute volume 0. Source OK is blocked by the incomplete source path, not
  the key. The RC 20/19/37 key fixes are committed; a TV retest is pending with
  the source/mute recovery.
- Takeover audio restore remains reboot-required for the hidden DSP state.
- Cold boot/replacement boot path, RF/DVB lock, CEC TX/RX, DVR, backlight
  feedback, IR repeat and amplifier mute remain unaccepted.
- Restore snapshot hash/state: source tree commit follows the report.

Report: NATIVE-REPLACEMENT-2026-11-08. Related: NATIVE-UI-2026-11-07,
REPLACEMENT-SYSTEM-2026-11-07, ALSA-CONTROL-2026-11-07, APP-MANAGER-CONFIG.
