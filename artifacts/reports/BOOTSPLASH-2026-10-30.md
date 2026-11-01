# Native boot splash — built, but the layer says no (2026-10-30)

> **Status: Historical negative experiment. Later quarter-screen boot paint invalidated a global impossibility conclusion; cold full-screen setup is still unverified.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Goal: paint an OmarchyTV-branded screen on the TV's OSD layer *before* the stock stack starts, to cover
the ~40–60 s between power-on and the web launcher. Small static ARM binary drawing straight to
`/dev/fb1` (no DirectFB, no libc beyond musl).

## What was built

`launcher/bootsplash/splash.c` → 17.7 KB static binary (`arm-linux-musleabihf-gcc -static -Os -s`).

* mmaps `/dev/fb1` (OSD_NORMAL2, 1920×2160 = 2×1080 frames, 32bpp, stride 7680, **BGRA**, corrected from this report's old RGBA claim) and paints **both**
  frames, so whichever buffer is scanned out shows the image.
* 5×7 bitmap font (A–Z, 0–9, space, `.` `-` `:`) drawn as scaled rectangles — no font file needed.
* Content: `OMARCHY TV` wordmark, accent rule, `VESTEL MB230`, `STARTING`. Options: `bootsplash "line1"
  "line2"` or `--blank`.
* Deployed to `/conf/bootsplash`.

2026-10-31 source audit: the legacy splash pixel writer also stored RGBA and is
now corrected to BGRA in source, with RGB hex palette constants unchanged. Only
offline compilation/memory checks were performed; deployed/recovery splash
binaries were not rebuilt or changed. Earlier colour claims used the old capture
decoder and are not reliable colour-parity evidence.

**It works** when the UI is already up — run it on a booted TV and `/dev/fb1` shows exactly the intended
screen (verified with `tools/tvshot.py`).

## What does not work: drawing *before* aurora

Wired into the hook immediately before `exec /vendor/com.nvt.idtv/aurora.elf`, the same binary draws
(logs `splash: drawn`) but the layer reads back **blank**, and the screen shows nothing:

* the write itself is not the problem (same binary, same node, works once the UI runs);
* the OSD plane is not displaying anything until the stock app / DirectFB initialises it — writes go to a
  buffer the hardware (or the driver's plane setup) does not present, and the next plane configuration
  resets it.

Re-tested with a 6 s hold before `exec` to rule out a race: still blank. So a pre-aurora splash would need
the OSD plane enabled by us first (`ntkdriver` plane params / display ioctls) — a separate investigation,
not a five-minute change.

## Decision

Hook reverted to the plain `exec`, with a comment pointing here. The binary stays in the repo and on the
TV as a working tool (useful if we ever learn the plane-init path, or for a "screen off" image). The
boot gap is covered by the stock UI, which is fine — the launcher appears as soon as the browser is up.

## What this tells us about option C (native overlay on `fb2`)

The same mechanism applies: a second process cannot simply paint an unused OSD plane and expect it to be
visible; the plane has to be enabled/configured by whoever owns the display. That makes the "native overlay
bar" idea (C) conditional on understanding the plane setup — worth one focused probe in the display driver
before planning anything bigger.
