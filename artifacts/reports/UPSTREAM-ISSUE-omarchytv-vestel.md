# OmarchyTV on a Vestel/Novatek TV (non-Android, no webOS/Tizen) — field report

> **Status: Draft field report only. Not evidence it was published, nor current TV state.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Thanks for OmarchyTV — we got the **webOS build** running on a 2019 Vestel-chassis Telefunken, on the TV
itself (no PC, no stick, no Android). Below is what it took, what broke, and a few suggestions that might
make this class of TV easier to support upstream. Everything here is from the "web app" side
(`webos/app`); the APK/Tizen/webOS packaging is untouched.

## The set

| | |
|---|---|
| TV | Telefunken D65U546A (65", 4K), rear label `D65U546A / 2199818` |
| Chassis | Vestel **MB230** (Novatek NT72671), NAND 512 MB, SPI 2 MB |
| Firmware | `0.51.52.3`, Linux **4.9.118**, glibc 2.23 (hard-float) |
| OS | **not Android**, not webOS/Tizen — a Vestel/Novatek "idtv" Linux stack |
| UI stack | `aurora` (DirectFB 1.7 + SawMan) for native UI, **Opera/OMI browser** for HTML |
| Browser | Chromium **67.0.3396.99** (`OPR/46.0.2207.0`, `OMI/4.13.5.431.SIERRA.170`) |
| Panel | 1920×1080, `devicePixelRatio = 1.5` → **1280×720 CSS px** |
| Tested build | your `webos/app` from **2.13.2-testing** (shared webOS/Tizen app) |

The TV is rooted (we control an unsigned config volume and run our own sshd/dialserver), which is how the
launcher is hosted — but the changes needed for OmarchyTV itself are small and would apply to any
"browser on a TV" platform.

## How it runs

* The launcher is served **by the TV**: busybox `httpd -p 8181 -h /buffer/launcher` serving
  `/buffer/launcher/omarchy/` — your `webos/app` (`index.html`, `app.js`, `data.js`, `dhh.js`, `compat.js`,
  `omarchy_themes.json`), the CaskaydiaMono font, all 28 theme wallpapers (downscaled to 1080p) and the
  5 intro videos.
* The TV's own app manager opens it through the set's DIAL command
  `<browserseturl><load url='http://127.0.0.1:8181/omarchy/' page='RC'/></browserseturl>`, once at boot
  (small boot hook with a 3-strikes watchdog) and again from a tile we added to the stock home screen.
* There is **no luna / no appcontrol**: anything that needs webOS services is inert, everything else works.

Full UA for the curious:

```
Mozilla/5.0 (Linux) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/67.0.3396.99 Safari/537.36
OPR/46.0.2207.0 OMI/4.13.5.431.SIERRA.170 Model/Vestel-MB230 VSTVB MB200 HbbTV/1.5.1 (+DRM;
TELEFUNKEN; MB230; 0.51.52.3; ; _TV_NT72671_2019;) SmartTvA/3.0.0
```

### Compatibility

Your `compat.js` was more than enough: of 28 feature probes, only `aspect-ratio`, `inset`, `:is()` and
touch events are missing — i.e. exactly the things your compat layer already emulates. No polyfill work
was needed beyond that. `video/mp4` (H.264) plays, so the **animated intro works**, and the canvas/ASCII
screensaver renders fine on this SoC.

## How the set was rooted (Vestel-specific, only needed to host the launcher here)

Rooting is **not** part of the OmarchyTV port — it is how we got a place to serve the page from on a TV
with no developer mode and no package manager. No exploit and no vendor keys were involved; the chain
relies on one unsigned flash volume and on the app's own debug path. Secure Boot is left intact.

1. **Find the mutable volume.** The NAND has `fs0`, `ap0`, `ap1` (signature-verified at boot through the
   `nvt_sign` volume) and **`apdat0`**, which is *not* verified and is mounted as `/conf`. Everything
   writable lives there.
2. **Own the app entry point.** `/conf` accepts a `remote.txt` (we put `0:127.0.0.1:x:x` in it). Its
   presence makes `appmgr` load `nvt.app.debug.ini`, where `enableMultiuser=n` means apps run as **root**,
   and whose idtv program is `run_aurora.sh`. That script first tries a CIFS mount to `127.0.0.1` — which
   fails instantly — and then runs the **local** `/conf/remotelf/aurora.elf` as root. That file is our hook.
3. **The hook** (a plain `/bin/sh` script) restores the real MAC and renews DHCP, mounts `devpts`,
   bind-mounts a patched `/etc/passwd` (root home `/tmp/tvh_root`, mode 700 so pubkey auth works), starts a
   static musl `dropbear` on `:22` with host keys stored in `/conf/remotelf`, a `telnetd` on `:2323`, and
   the TV's own DIAL server, then `exec`s the real UI. Net effect: the set boots completely normally,
   with a root shell as a side effect, persistent across power cycles.
4. **Getting the first write in** uses the TV's own recovery channel: a FAT32 stick with a U-Boot
   auto-script called `kylo_usb_update.scr` (plain text, < 8 KB, last line `%`), executed when you hold
   **OK** on the remote while plugging the power back in. The script does `fatload` +
   `ubi part apdat0` + `ubi write`. We first used the same channel *read-only* to dump the whole NAND to
   the stick (`nand read` → `n00..n63.bin`), then rebuilt a `ubifs` image offline with `mtd-utils`
   (`mkfs.ubifs`) from that dump, so nothing vendor-supplied is needed.
5. **Only `apdat0` is ever written.** The signed volumes, the kernel, U-Boot and the SPI flash are
   untouched, and the pristine dump plus a restore script are kept — a full NAND restore is the last
   resort. The same trick applies to other MB230-based sets (Telefunken/Vestel/Hisense-branded), but the
   restore path should be prepared *first*.

The launcher then lives in `/buffer` (also writable), is served by busybox `httpd`, and the TV's DIAL
command opens it — so the root only had to provide a shell, an HTTP server and a way to trigger the URL.

Happy to write this up in more detail (exact scripts, `ubifs` parameters, the dump/restore procedure) if
anyone wants to try it on another MB230-based set — it is all built from the TV's own dump, nothing
vendor-supplied and no copyrighted firmware involved.

## The three things that actually needed care

### 1. Scaling / viewport (the important one)

The upstream meta is `width=1920, height=1080, initial-scale=1.0`. On this browser that makes
`window.innerWidth` report **1920** while the visible viewport is **1280×720 CSS** (1920×1080 device at
DPR 1.5). Your own `fitStage()` then computes `min(innerWidth/1920, innerHeight/1080) = min(1.0, 1.0) = 1.0`,
so the 1920×1080 stage is drawn 1:1 and the bottom of the UI is clipped.

Fix: `<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">` and let
`fitStage()` do its thing (it lands on 0.667 and the stage fills the panel 1:1). Worth mentioning: our
first attempt added our own `transform: scale()` on `body`, which double-scaled everything into the
corner — that's how we discovered `fitStage()` existed. Deriving the factor from
`screen.width / devicePixelRatio` also works if a platform lies about `innerWidth`.

### 2. 12 h vs 24 h clock

`clockFormat: "auto"` is documented as "12 h in English, 24 h otherwise", and this browser reports
`en-US` — so a TV in the Netherlands showed `2:06 PM`. We added a `OMARCHY_PREFERS_24H` flag that the
platform sets, and `use24h()` consults it for `auto`.

Two details that may be generally useful: this browser's
`Intl.DateTimeFormat().resolvedOptions().timeZone` returns a bare **`CET`** (not `Europe/Amsterdam`), so a
`^Europe/` test misses it — accepting the legacy zone names (`CET/CEST/MET/EET/WET/GMT/UTC`) fixed it.
Fallback we used: render 13:00 with `toLocaleTimeString()` and look for `am/pm`.

### 3. Script caching

This browser caches the JS hard. After editing `app.js`/`platform.js`, `index.html` must bump a `?v=N` on
the script tags or the old code keeps running (it cost us two confusing rounds). Also: a URL with **more
than one query parameter** gets mangled by the TV's browser/app path — `?theme=last-horizon&fix=1`
produced the theme id `last-horizonfix`; single-parameter URLs are fine.

## What we changed (about 25 lines + one new file)

`platform.js`, loaded before `data.js`/`app.js`:

```js
window.OMARCHY_PLATFORM   = "vestel";
window.OMARCHY_VESTEL_APPS = [ /* TV, Sources, Stock portal — what this set can actually do */ ];
window.OMARCHY_VESTEL_INPUTS = [ /* TV, HDMI 1-3 */ ];
window.OMARCHY_LAUNCH = function (app) { /* tile -> local HTTP helper -> TV key path */ };
window.OMARCHY_PREFERS_24H = /* timezone based */;
window.OMARCHY_HIDE_UNSUPPORTED = true;   // hide System rows that need webOS/AI services
```

`app.js` hooks (all tiny, same shape as your Tizen adapter):

1. `launchApp()` → `if (window.OMARCHY_LAUNCH) { window.OMARCHY_LAUNCH(app); return; }`
2. after the Tizen block: `if (platform === "vestel" && window.OMARCHY_VESTEL_APPS) DEFAULT_APPS = …`
3. `TV_INPUTS` → `(platform === "vestel" && window.OMARCHY_VESTEL_INPUTS) || [ …defaults ]`
4. `use24h()` consults `OMARCHY_PREFERS_24H` for `auto`
5. System rows can be skipped with a `skipVestel: true` flag (we filter when `OMARCHY_HIDE_UNSUPPORTED`)
6. idle "turn off" → `window.OMARCHY_STANDBY()` when provided

`index.html`: viewport meta, versioned script tags, and a wired-network glyph instead of the Wi-Fi arcs.

## What works, what can't

**Works:** home + dock, workspaces 1–5, all 28 themes and their wallpapers, animated intro, the Aquarium
screensaver, text size, transparency, panel toggles, weather (Open-Meteo + real city search via
`geocoding-api.open-meteo.com`), the app tiles, source switching, and handing the screen back to the TV.

**Inert (no platform services):** launching other apps (the set has none we can start; streaming apps are
disabled in its config), PC-link pairing, AI credits, plugins, Ambilight, Cinemarchy/YouTube embed,
notifications/MCP. We hide those rows rather than show dead entries.

**Our own plumbing, for context:** the set's `<command>Standby</command>` is ignored on this build, but the
*power key* suspends it even with the page in front — and the page does not survive suspend, so a small
watcher re-opens the launcher after wake. The "pc" panel points at a local CGI that reports the TV's own
`/proc` stats (CPU/RAM/uptime), since there is no PC in the loop.

## Suggestions (all optional)

1. **A generic "browser platform"**. The Tizen adapter already does the right thing; a documented
   `OMARCHY_PLATFORM = "generic"` (or `"vestel"`, `"hisense"`, …) with:
   `OMARCHY_LAUNCH(app)` for launching, injectable app/input lists, and an optional `OMARCHY_STANDBY`,
   would let any TV browser host the launcher without patching `app.js` at all.
2. **Viewport**: prefer `width=device-width` and let `fitStage()` scale, or compute from
   `screen.width / devicePixelRatio` when `innerWidth` looks wrong. (Would have saved us the clipping bug.)
3. **Clock auto**: derive from `Intl`/timezone rather than the UI language, with the legacy-zone caveat.
4. **Configurable System rows**: a platform-supplied list of unsupported actions would beat our
   `skipVestel` flags.
5. **Status panel hook**: the `cfg.pcHost` indirection already let us point the panel at the TV itself —
   documenting that as "any URL returning the status JSON" would make it reusable.

## Screenshots

Attached: contact sheet (home, workspace 2, screensaver, workspace 4) — plus, if useful, individual shots of
the system menu, the theme list and a 24 h Dutch home screen.

## Licence

Everything we changed is a shim around your MIT code; your LICENSE/NOTICE are kept in the fork. Happy to
turn any of the above into a merge request (a `generic` platform stub + the viewport/clock tweaks) if you'd
like it upstream.
