# Controlled-ownership first-frame scanout test — 2026-11-06

Owner authorization: **“you have full access to the TV”** (2026-11-06). Temporary
controlled display ownership with stock Aurora stopped, full fb1 state restored
and stock brought back by the watchdog reboot path. No persistent change.

## Pre-state

```text
stock: run_aurora.sh 1711, aurora.elf 1781, appmgr 1579, zombie [aurora.elf] 2204
uptime 64649 s
fb1 OSD_NORMAL2: 1920x2160 virtual, stride 7680, yoffset 0,0
region[0..1] 0/0, MAU 0
/conf/oa_boot and /conf/oa_fail absent; /conf/oa_safe and coredumpnoreset present
```

`fb-restore save /dev/fb1` produced the full 16,588,800-byte pre-state file.

## Controlled run

- `/buffer/wdtctl disable` → `watchdog disable -> sent`.
- SIGTERM to the exact live `/vendor/com.nvt.idtv/aurora.elf` PID 1781; no stock
  Aurora or `run_aurora.sh` remained in `ps`.
- Staged `/buffer/oa-scanout`: hash-verified ARM `openaurora`
  `025d1fb0…`, `oa-client` `f3d20a61…`, `mod_display_fb.so` `b7ab1747…`,
  `fb-restore` `afa1dbe1…`, config with `display.restore_snapshot=1`,
  `display.test=1`, `display.mirror=0`, `display.set_region=1`,
  `display.pan=0`, `display.verify_scanout=1`.
- `start display_fb` saved the full snapshot, set the region and painted the
  self-test band. Log:

```text
present: scene=5ms mirror=0ms sync=0ms pan=13ms rect=0,0 1920x135
self-test: magenta band painted
/dev/fb1: scanout verified at start: yoffset 0 region 1920x1080 at 0,0 mau 0
```

## Stability evidence

Three fb1 captures two seconds apart while stock was absent:

```text
active frame 0 PNG sha256 752e91368a6d921dfc25603be8aad8be25e4fd8be7e2402e9955cc2eb03275d4
             (a, b and c identical)
inactive frame 1 PNG sha256 75d32c1eabd7934cb0f1fb02fb7c53cb90af8c316e9594658a749d0ccee7e5c5
active frame: (960,10) = 255,0,255 magenta; inactive frame: 0,0,0
```

Raw active-frame read (`dd bs=7680 count=1080`, frame 0) three times, one second
apart:

```text
656b0caf747b6546312818c70dc2d1aaf3b10cc60778f5b1dc8a7c52da47f9cb  -
656b0caf747b6546312818c70dc2d1aaf3b10cc60778f5b1dc8a7c52da47f9cb  -
656b0caf747b6546312818c70dc2d1aaf3b10cc60778f5b1dc8a7c52da47f9cb  -
```

Because stock Aurora was not running, the held frame could not be a
stock-repaint artifact: the active buffer was byte-identical across the hold,
and the driver reported the committed yoffset, our region and unchanged MAU.

Captures are preserved under `artifacts/analysis/controlled-scanout-2026-11-06/`.

## Restore and recovery

- `stop display_fb` needed the known retry (`cleanup pending` then `ok`);
  `quit` exited cleanly, no `openaurora` remained.
- Our magenta band is gone: row 10 of both frames reads `0 0 0 0 0 0 0 0`.
  A full post-stop snapshot differs from the earlier manual pre-state file in
  3 bytes in one stock pixel region (the stock UI changed between the manual
  save and display start); region/pan/MAU are the same and the band area is
  restored.
- `/buffer/oa-scanout` was removed and verified absent.
- `/buffer/wdtctl set 10` rebooted the SoC. Stock returned: uptime ~28 s,
  appmgr 1582, `aurora.elf` 1784 (zombie 1962), fb1 1920x2160 stride 7680,
  no `openaurora`, `/conf/oa_boot`/`oa_fail` absent and `oa_safe`/
  `coredumpnoreset` present. A post-boot fb1 capture is non-blank.

## What this establishes and what it does not

This is a stable **controlled-ownership framebuffer/scanout-path** test: with
stock stopped, our first frame stayed in the active OSD buffer unchanged, and
the recovered pan/region/MAU state verified. It is stronger than the earlier
device-level test under the stock owner.

It is still not independent panel-scanout proof: no owner visual confirmation
or HDMI capture was recorded. It is also not cold startup — fb1 was already
vendor-initialized before stock was stopped, the watchdog was temporarily
disabled and restored by reboot, and no MCU authority or clean handback beyond
this reboot was established.

The build/driver state is otherwise unchanged; no boot hook, persistent module
or `/conf` flag was added. The owner may confirm the magenta band was visible if
they observed the set during the hold; until then the panel effect remains
owner-observation-pending.

## Confirmed full-screen run — 2026-11-06

A second controlled run replaced the band with `display.test=2`, eight
full-screen vertical bars, and held it for visual confirmation.

- Same staged core/module hashes (`openaurora 025d1fb0…`, `mod_display_fb`
  `5422a86c…`); watchdog disabled, stock Aurora 1784 stopped, fb1 snapshot
  saved.
- Start log:

```text
present: scene=39ms mirror=0ms sync=0ms pan=14ms rect=0,0 1920x1080
self-test: 8 full-screen colour bars painted
/dev/fb1: scanout verified at start: yoffset 0 region 1920x1080 at 0,0 mau 0
```

- Two captures 5 s apart were identical
  (`a29ccecc1d671106…`, `scanout-bars-a_0.png` / `-b_0.png`). Sampled at
  y=540 across the panel:

```text
x 120 white, 360 yellow, 600 cyan, 840 green,
x 1080 magenta, 1320 red, 1560 blue, 1800 black
```

- **Owner visual confirmation:** asked to compare the panel against that
  left-to-right bar order, the owner answered **“Yes, I see the bars.”**
- Stop restored fb1 (row 10 reads zero), staging was removed, and
  `wdtctl set 10` rebooted. Stock returned: uptime ~27.7 s, appmgr 1574,
  `aurora.elf` 1774 (zombie 1945), fb1 1920x2160 stride 7680, no
  `openaurora`, `oa_boot`/`oa_fail` absent, `oa_safe`/`coredumpnoreset`
  present, and the post-boot capture is non-blank.

This closes the independent **panel-scanout and full-screen geometry** check for
the controlled first frame: the owner saw the eight full-screen bars in our
order and the captured active frame matches the same layout. It is still not
cold independent startup, vendor UI pixel parity or MCU/standby authority.
