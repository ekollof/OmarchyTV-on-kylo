# Plan: replace the stock launcher with an OmarchyTV-style UI (2026-10-30)

> **Status: Superseded initial plan. Web milestones P0–P3 happened later; boot is now disabled and native openaurora is the active direction.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Status: **P0 done** (`P0-BROWSER-PROBE-2026-10-30.md`), nothing installed. The probe
confirmed the web route: Chromium 67, 1280x720 CSS viewport, DOM arrow/OK/digit events,
same-origin CGI, `EXIT` → stock TV, `browserseturl` → back into the launcher.

Extra requirement from the owner: the launcher must also let you **watch the inputs**
(HDMI 1-3, AV/composite, VGA if present) — so a "Sources" tile is mandatory, and the
source-switch path is currently an open item (§6, P1).

## 1. What OmarchyTV actually gives us

`gitlab.com/piermark/omarchytv` (MIT, v2.13.x) ships three things:

| artifact | fits this TV? |
|---|---|
| `omarchytv-release-*.apk` — Android TV launcher (`minSdk 24`) | **No** — our set is Vestel/Novatek Linux, not Android. Nothing to install. |
| `webos/app` — a plain **HTML/CSS/JS web app** (`index.html` 23 KB, `app.js` 125 KB, `data.js` 33 KB, `omarchy_themes.json` 15 KB) | **Yes — this is the target.** It is a self-contained web UI with D-pad navigation, 28 themes, workspaces, app grid. |
| `webos/app/rooted/omarchytv-home.sh` — home-button redirection on *rooted webOS* via `luna-send` | **No**, but it is a good **model** for what we need (see §3). |

The decisive detail: the web app already carries a **legacy compatibility layer**
(`compat.js`) for **Chrome 38** (webOS 3.x) — polyfills for `fetch`, `Object.assign`,
CSS custom properties, CSS grid, `gap`, `inset`, `aspect-ratio`. Key handling falls
back to `keyCode` when `KeyboardEvent.key` is missing (`LEGACY_KEYS` = 8/13/27/37–40,
digits, plus webOS 461 and Tizen 10009). Our OMI/Opera browser is from 2019 and is
almost certainly **newer** than that baseline, so this is a realistic port rather than
a rewrite.

Platform-specific surfaces we must replace (they are isolated, by their design):

| in the app | today | for us |
|---|---|---|
| `window.OMARCHY_PLATFORM` | `webos` / `tizen` | add `vestel` branch |
| `launchApp(appId)` | luna `applicationmanager/launch` / Tizen `appcontrol` | HTTP call to our own CGI → DIAL `POST /apps/<name>` or key sequence |
| app list | hard-coded LG ids + Tizen discovery | our list (`dial_appname_to_prefixurl_list.xml`, launcher tiles we can actually start) |
| TV/HDMI sources | webOS `com.webos.app.hdmiN` | `KeyAux` (1056) + arrows/OK, or the source menu |
| weather / PC bridge | HTTP to open-meteo / `:8765` | keep (works offline-degraded) |
| screensaver (`tte.js`) | — | optional, drop if the engine is too slow |

## 2. The good news on infrastructure (already verified today)

* **Static server needs no code**: busybox in `/bin` includes `httpd`, and
  `/usr/sbin/httpd` is a symlink to it. `httpd -h /buffer/launcher -p 8181` is enough,
  including CGI (shell) for actions.
* **Space**: `/buffer` has **209 MB free**; `/conf` has ~2 MB free. The launcher
  (code + a few themes) goes in `/buffer/launcher`, only the hook/flag files in `/conf`.
* **Remote observability**: `tools/tvshot.py` (framebuffer screenshot), DIAL key/command
  POSTs (proven), `/conf/tvhack.log`. We can verify every step without standing in front
  of the TV.
* **Zero firmware risk**: everything lives in `/conf` + `/buffer` (both unsigned,
  writable). No signed volume is touched, no kernel/bootloader work.

## 3. The hard part: switching between launcher and stock aurora

This is the design question worth solving *before* writing UI code. Three models:

### Model A — "launcher is a web app on top of aurora" (recommended first)
```
boot → hook (sshd, dial, httpd on 8181) → aurora (stock UI, owns fb0..fb4)
                                           │
        our helper (shell loop, waits for aurora + browser)
                                           │
                    DIAL browserseturl  ──► OMI browser opens http://127.0.0.1:8181/
                                           │
                       launcher page (the OmarchyTV UI)
                       [ TV ] tile ── CGI ──► DIAL key EXIT(1037) → back to aurora
```
* Enter: automatic at boot (flag), plus via `curl http://<tv>:8181/cgi-bin/launcher`
  from any device on the LAN (and from the PC plugin later).
* Exit: the browser's own EXIT/BACK returns to aurora — one key, always the same,
  and it is the stock UI, not something we built.
* Risk: **low**. If the page is broken, EXIT (or a reboot with the flag off) restores
  everything. The stock UI is never removed.
* Weakness: to get *back* into the launcher from stock aurora we need an entry point.
  Candidates to test (P1): the stock launcher's own app/browser tile, `POST /apps/vr`
  (its `prefix_url` is "NO URL FOR SMARTCENTER"), a DIAL command, or a time-out /
  idle re-open. If none works, either the phone/PC is the entry point, or we go to
  Model C for a real "Home" experience.

### Model B — "register our launcher as an app of the platform"
Understand first whether `appmgr` + the stock launcher can be taught about a new app:
`nvt.app.debug.ini` already selects the idtv program, and the launcher's tiles come
from an app list we have not fully mapped (open TODO question). If a "web app" entry
can be added there, the stock UI gets an **OmarchyTV tile** and switching becomes
native. Research item, not an assumption.

### Model C — "own the screen" (true replacement)
Our own process draws the launcher (DirectFB client or direct fb writes) and starts
aurora only when the user picks TV. Switches are process switches; needs input
(`/dev/input` only has `mice`, IR goes through DirectFB/ICE), audio/video handover,
and app launching. Highest effort, highest risk, best result. **Only after A proves
the UI and B proves we cannot get a native entry point.**

Rule of thumb: **A for the UI work, B for the integration, C only if both stall.**

## 4. Test harness (how we prove each step remotely)

| what | how |
|---|---|
| did the page load / render | `tools/tvshot.py --layer 0/1` (screenshot), plus a beacon CGI that logs UA + hits to `/conf/tvhack.log` |
| which keys reach the page | page posts `keydown` code+key to `/cgi-bin/beacon?k=…`; we read the log |
| engine capabilities | one CGI that records `navigator.userAgent`, feature-detect results (ES6, `fetch`, CSS grid/vars, `WebP`, `canvas`, `localStorage`, `requestAnimationFrame`, `video/webm`) |
| app launching | the real thing: DIAL POST from CGI, then screenshot |
| switch in/out | screenshot before/after each action; `ps | grep omi_worker` |
| regressions on stock UI | press EXIT, screenshot the launcher, compare with the reference shots in `artifacts/protocol/` |

## 5. Phases, exit criteria, rollback

### P0 — capability probe (no persistent change, ~30 min)
Serve from the laptop (or a temp `/buffer/probe`), open via DIAL, capture:
1. `navigator.userAgent` + engine feature matrix (the beacon).
2. Which remote keys arrive, and as what (`keyCode`/`key`) — navigation, OK, EXIT,
   BACK, MENU, digits.
3. Whether a loaded page survives, and what EXIT does (back to stock UI?).
4. Whether `http://127.0.0.1:8181` is reachable from the page (same-origin CGI).
**Exit:** we know the engine, the key map, and the exit semantics.
**Rollback:** nothing persisted; EXIT/reboot.

### P1 — mock launcher (our own page, ~50 lines)
Three tiles (TV / YouTube / Launcher settings), driven by the real key map, with one
CGI (`/cgi-bin/act`) that does the DIAL work.
**Exit:** boot → launcher on screen → navigate → "TV" → stock UI → back into the
launcher by *some* repeatable route (P0/P1 finds it; if not, escalate to Model B/C).
**Rollback:** `/conf/no_launcher` flag, EXIT key.

### P2 — host it on the TV + boot integration
`/buffer/launcher` + `httpd -p 8181` + hook changes behind `/conf/launcher_enable`
(default **off**), plus a supervisor: *auto-open once per boot, and only if the last
open registered a beacon successfully* (a marker file prevents a boot loop on a
broken page). Watchdog rule: 3 failures → disable until re-enabled.
**Exit:** reboot with the flag on lands on the launcher; with the flag off, stock
behaviour unchanged; failing page leaves a usable TV.

### P3 — port OmarchyTV
Vendor the webOS app as a **patched fork** (`launcher/` in this repo, `NOTICE`
+ upstream tag recorded), add the `vestel` platform branch, our app list, our keymap,
and 1–3 themes/wallpapers (1080p, not 4K, to keep it snappy). Keep upstream diffs
small and documented so we can re-sync.
**Exit:** the real UI on the TV, all tiles functional or explicitly disabled, no
console errors in the beacon log, screensaver optional.

### P4 — optional: native path (Model C)
Only if P1 shows the browser route cannot be the "Home" experience.

## 6. Risks / open questions

0. ~~**Input switching (owner requirement).**~~ **Resolved in P0b**: `KeyAux` 1056 opens the
   native "Bronlijst" (TV / BACK AV / HDMI1-3 / YPbPr / VGA-PC / Wireless Display), it opens
   on the current source and wraps; `1056, DOWN x3, OK` = HDMI2, verified by the OSD banner.
   Keys must go as a tight burst (~0.25 s) because the list is transient.
   → A "Sources" tile can simply press 1056 and let the user navigate the native list.

1. **Browser engine age** — 2019 OMI; `compat.js` targets Chrome 38 so the odds are
   good, but the launcher's CSS/JS volume (125 KB app.js) on a weak SoC may be slow.
   Probe first; if it is slow, cut features (no animations, fewer themes).
2. **Who owns the screen** — aurora is the DirectFB master; a page shown by OMI is
   composited by aurora, so no conflict *in Model A*. In Model C this becomes the
   central problem.
3. **Getting back in** (§3, Model A weakness) — the one unresolved UX question.
4. **App tiles that do nothing** — most streaming apps are disabled in
   `nvt.app.debug.ini`; the launcher must only show what actually starts.
5. **Standby** — the TV sleeps ~3 min on no-signal. The browser counts as activity?
   Must test: does the launcher page keep the TV awake or does it need an input event?
   If it sleeps, add a wake trigger (WoL is proven) or a synthetic key every N minutes.
6. **Audio/video handover** — if the launcher plays a wallpaper video / sound, we must
   release the audio path when switching to TV.

## 7. Decisions needed before touching the TV

- [ ] Confirm the target experience: launcher as the **boot screen** (auto-open) vs.
      something you open on demand from a phone/PC.
- [ ] Accept **Model A** for the first iteration (stock aurora stays, browser on top)?
- [ ] Port scope: full OmarchyTV (themes, workspaces, screensaver) or a slim
      "Omarchy-styled home" with the tiles that work on this set?
- [ ] Is a **fork in this repo** of the MIT web app fine (with attribution), or do you
      want it upstream-able as a `vestel`/`novatek` platform contribution?

Nothing in P0 changes the TV's state: it needs one temporary HTTP server (laptop or
`/buffer`) and DIAL to open a page we already know works.
