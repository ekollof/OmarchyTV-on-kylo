# Real-platform takeover acceptance — 2026-11-07

Status: first owner-authorized hardware run of the **32-module real-platform
bundle**. Visible output and native UI were observed from fb1 captures. This is
not a complete-replacement claim. The process-only restore brought stock back
but left the soundbar silent; a forced `reboot -f` was required to recover
audio. That regression is part of this record.

Audio topology on this unit: the soundbar is fed by **optical S/PDIF**, and the
TV's internal speakers are intentionally muted. “Audio back” below means
owner-confirmed soundbar output over optical; it does not validate TV speakers,
the amplifier mute path, or CEC audio.

## Setup

```text
bundle      /buffer/oa-full-20261107
tar         d659cf78040acfd74156827f6c2a741713ce83176c7d8cea1daf67cb3ad3932c
plat        real mod_plat.so, plat.drm_init=0
display     /dev/fb1, cold_init=1, set_region=1, mirror=0
ui          ui_vendor active by default; ui_omarchy loaded explicitly
stock       appmgr 1574; vendor aurora.elf 1778 before the run
```

The start bundle tar was
`d659cf78040acfd74156827f6c2a741713ce83176c7d8cea1daf67cb3ad3932c`
with `mod_ui_omarchy.so` `405c6c0a...`. During the run the native module was
hot-swapped to `359e7aa9...` (parent-filtered launcher list + action logging)
and the final uploaded bundle is
`1038bada12dcf04691ce010dab80d699be05105ae52484316dcd44de2d802d06`.

`/conf/oa_safe` existed from an earlier failed-run guard. It was moved to
`/conf/oa_safe.takeover-bak` for the window and restored afterwards; leaving it
in place makes `oa-supervise` fall back before starting the child.

## Start

`takeover-full.sh start` completed the manifest/`--check` preflight, disabled the
watchdog, stopped stock and launched:

```text
oa-supervise 10480
openaurora   10483
```

All 32 modules started, including real `plat`, `display`, `audio`, `tuner`,
`cec`, `ticker`, `ui_engine`, `ui_vendor`, `ui_theme/profile/menu/list/nav`,
`lua`, and the panel/audio/IR modules. Key state:

```text
display 1280x720 32bpp 2 frames
panel percent 100
audio enabled 1 volume -1
tuner ... lock 0 ... no RF
cec owner 1 audio 1 supported 0 local 1 enabled 0 open 0
```

`plat: Hal_Video_DrmInit skipped; preserving the vendor OSD` and
`display` came up on the vendor-initialized OSD. The captured fb1 shows the
vendor-style portal at 1280x720:
`artifacts/analysis/takeover-2026-11-07/vendor-portal-1280x720.png`.

The stock `fb1` also reports `1280,1440` / stride 5120 / 32bpp, so 1280x720 is
the current stock OSD mode on this unit, not a takeover regression.

## Native shell on the TV

`ui_omarchy` was loaded/started and selected (`skin omarchy`). The native shell
painted on the live fb1:

```text
native-home-1280x720.png     OmarchyTV home, source 5, CEC on
native-apps-1280x720.png     Apps 19: Netflix, Prime Video, YouTube, Canal
                             Digitaal, TV VLAANDEREN, Télésat, Digiturk Play,
                             Rakuten Tv, ...
native-settings-1280x720.png Settings, current source 5, CEC on
```

The app list uses `read_parent(0)` and filters deleted/out-of-sync rows; the
earlier `read_all` path exposed 54 hidden/duplicate rows.

Native SELECT was exercised on the TV:

```text
[944160] INFO ui_omarchy action kind=0 index=1 name=Prime Video
```

That is `OA_ACTION_SELECT`; there is still no in-process action consumer, so no
application process was launched.

One transient was observed: the first `start ui_omarchy` returned `failed (-1)`
while the engine subsequently painted and logged `started ui_omarchy`. A clean
`unload ui_omarchy` → `load` → `start` → `skin omarchy` succeeded; the first
failure reason is not yet isolated and is recorded as an open defect, not
hidden.

## Suite reload

`reload-suite` produced a clean core exit; the supervisor stayed and relaunched:

```text
before: supervisor 10480, core 10483
after:  supervisor 10480, core 11346
```

The relaunched suite started all modules again and the vendor portal returned
(`vendor-portal-after-reload-1280x720.png`, fb1 md5 `bcc9015d60df70deb861213e0ed72ea1`).

## Restore and audio regression

`takeover-full.sh restore` sent `quit`; the supervisor re-armed the watchdog and
exec’d the vendor fallback:

```text
10480  1  /vendor/com.nvt.idtv/aurora.elf
1574  1363  /vendor/com.nvt.appmgr/com.nvt.appmgr --applist=...nvt.app.debug.ini
```

No `openaurora` or `oa-supervise` remains. `/conf/oa_safe` is restored. The
stock UI returned and post-restore fb1 was 1280x1440 / 32bpp.

**However, the soundbar was silent after the process-only restore.** Killing the
stock Aurora process and letting appmgr/stock restart did not recover audio. A
forced `reboot -f` was issued; the set returned with appmgr 1577 and stock
aurora.elf 1769, fb1 1920x2160, and the owner confirmed **optical soundbar audio
was back**. The TV's internal speakers remain intentionally muted, so this is not
speaker/amp acceptance.

The exact broken state is not isolated. The most likely area is the real
`plat` audio prelude / digital S/PDIF routing (possibly amp state) that the
vendor process restart does not reinitialize. Until an audio restore path is
recovered, a takeover must be treated as requiring a reboot to guarantee
optical soundbar audio.

## What this accepts

- The real `plat` path with `plat.drm_init=0` starts in one process with the
  full 32-module set and produces a visible vendor OSD/portal.
- `ui_omarchy` can be loaded and selected at runtime on the TV, paints the
  native home/apps/settings screens, reads the live launcher/settings values,
  and emits `OA_ACTION_SELECT` for a real app.
- `reload-suite` relaunches the whole core under the same supervisor without a
  reboot.
- Process/UI restoration works without reboot, but **audio restoration does
  not**: this run required `reboot -f` and owner confirmation that audio was
  back.

## Still open

- No controlled cold-boot test before the forced recovery reboot;
  `display.cold_init` did not fire because stock fb1 already had a valid
  1280x720 var. The post-reboot stock fb1 is 1920x2160, so the takeover/restore
  also left a non-stock 1280x720 OSD mode until reboot.
- No app process launch/action consumer; native Apps emits events only.
- The transient first `ui_omarchy` start failure needs diagnosis.
- Soundbar audio (optical S/PDIF) is broken by the takeover/process restore and
  only recovered by forced reboot; this is a release blocker for future
  hardware windows. TV speakers are intentionally muted, so this is not
  amplifier/speaker acceptance.
- RF/tuner lock, DVR, CEC TX/RX, amp mute gate, backlight confirmation, IR
  repeat/OK and full UI/remote parity remain unaccepted.
- Persistent boot/app-manager installation and the `/conf/oa_safe` policy are
  separate gates; no boot hook was changed.

Report: TAKEOVER-ACCEPT-2026-11-07. Related: REPLACEMENT-SYSTEM, NATIVE-UI,
APP-MANAGER-ENTRY, COLD-DISPLAY-REPAIR.
