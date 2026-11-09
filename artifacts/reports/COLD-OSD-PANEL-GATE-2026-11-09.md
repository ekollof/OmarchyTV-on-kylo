# Cold OSD/panel gate — 2026-11-09

Status: **cold-profile display gate run with stock stopped; OSD captured;
stock restored and force-rebooted. Not a power-on boot without stock and not
a persistent replacement.**

## What ran

The 34-module `build-tv-v3/cold-replacement` bundle was staged at
`/buffer/openaurora-cold-gate` and started under `oa-supervise` after stopping
stock `aurora.elf` (temporary takeover). The cold profile uses:

```text
plat.drm_init = 1
display.cold_init = 1
display.osd_reinit = 1
display.restore_snapshot = 0
panel.standby = 1
panel.power_on_start = 1
spi.enable = 1
source.enable = 0
audio.source_enable = 0
cec.enable = 0
```

The bundle contains 34 modules; the cold autoload list starts 32 of them
(`audio_source` and `ui_omarchy` are bundled but not autoloaded). Preflight
`--check` passed and the manifest verified before the run.

## Evidence

```text
[DRM Init] Step 1: Open DRM Device: drm_card_id = 13
[DRM Init] open drm pass
[DRM Init] fb_id = 98
[DRM Init] crtc_id = 26
[DRM Init] plane_id[7] = 40, [6] = 54, [5] = 55, [2] = 56,
           [1] = 59, [0] = 60, [3] = 61, [4] = 62
[DRM Init] Step 3: Init Kernel Mode Setting - End

panel percent 100
fb1 virtual_size=1920,2160 stride=7680 bits_per_pixel=32 pan=0,1080
ui_vendor present: scene=51ms mirror=0ms sync=0ms pan=7ms
                  mirror=0 rect=0,0 1920x1080
ui_vendor painted portal: 5 rail items, 12 cards, band 0 rail 2 card 0
oa-supervise: host loop alive, first present acknowledged
```

`tvshot` read fb1 (OSD_NORMAL2): frame 0 blank, frame 1 non-blank with the
vendor portal/apps grid at 1920x1080. The capture proves OSD-layer content;
the DRM init/pan and panel-power path ran, but physical panel scanout still
needs the owner's visual confirmation.

## Restore

The session was quit, `/conf/oa_safe` restored, the staging directory removed
and the TV force-rebooted. Stock returned:

```text
up 3 min
appmgr 1573
aurora.elf 1776
fb1 1920,2160
/conf/oa_safe present
watchdog set 10s
```

## Limits

- This is a **stopped-stock cold-profile start**, not an unassisted power-on
  boot. No boot hook or persistent replacement was installed.
- 32 modules autoloaded (34 present in the bundle); `audio_source` and
  `ui_omarchy` are not part of this cold UI profile.
- The fb1 screenshot is OSD-layer evidence, not independent proof of the
  physical panel/scanout or of every plane.
- `source.enable`, `audio.source_enable` and `cec.enable` remain 0.
- True cold boot, boot-hook persistence and stock-UI full parity remain open.
