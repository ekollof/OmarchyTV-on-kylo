# P3 — OmarchyTV running on the TV (2026-10-30)

The real launcher (upstream OmarchyTV, MIT) now boots on the Vestel MB230, replacing the P1 mock
as the default screen. The mock stays at `/` as a fallback; the port lives at `/omarchy/`.

## What was ported

Upstream `webos/app` (the shared webOS/Tizen build): `index.html`, `app.js` (125 KB),
`data.js`, `dhh.js`, `compat.js`, `omarchy_themes.json`, plus two assets:

* `fonts/caskaydia_mono_{regular,bold}.ttf` (73 KB, from `launcher/app/src/main/assets/fonts`)
* `wallpapers/last-horizon/01-eyes-wide.webp`, `wallpapers/tokyo-night/01-liftoff-sunset.webp`

Repo layout: `launcher/omarchy/` (≈1.3 MB). Deployed to `/buffer/launcher/omarchy/`.

## Three small patches

1. **`platform.js`** (new, loaded from `index.html` after `compat.js`):
   * `window.OMARCHY_PLATFORM = "vestel"`
   * `OMARCHY_VESTEL_APPS` — our three tiles (TV / Sources / Stock portal)
   * `OMARCHY_VESTEL_INPUTS` — TV, HDMI 1–3
   * `OMARCHY_LAUNCH(app)` — maps a tile to `/cgi-bin/act` (`a=tv` = EXIT, `a=sources` = native
     Bronlijst, `a=apps` = stock portal)
   * boot handshake + heartbeat for the hook watchdog (`a=boot-ok`, `a=alive` every 5 s)
2. **`app.js`**: two hooks — `launchApp()` calls `window.OMARCHY_LAUNCH` when present, and the
   vestel app/input lists replace the LG ones (same pattern as the Tizen adapter).
3. **`index.html`**: viewport meta → `width=device-width, initial-scale=1`.

## The scaling trap (worth remembering)

The app renders a fixed **1920×1080 `#stage`** and scales it itself in `fitStage()`
(`min(innerWidth/1920, innerHeight/1080)`). This TV gives a **1280×720 CSS viewport at DPR 1.5**
(1920×1080 device px), so:

* with the upstream meta (`width=1920`), `innerWidth` reported 1920 → `fitStage` computed 1.0 →
  the UI overflowed and was clipped at the bottom;
* adding our own `transform: scale()` on `body` double-scaled it (0.667²=0.444, UI in a corner);
* the fix is just **`width=device-width`** and letting `fitStage()` do its job. Verified from
  inside the page: `iw=1280 ih=720 sw=1920 dpr=1.5` → stage scaled 0.667 → fills the panel 1:1.

## Verified on the TV

| check | result |
|---|---|
| boots into OmarchyTV | yes — hook opens `/omarchy/`, page reports `boot-ok`, opener stops (attempt 6) |
| renders | full UI: top bar (workspaces 1-5, clock, SpaceX TV pill, status), OMARCHY TV block-logo panel, weather (Open-Meteo, 17°), `omarchy · pc` / `ai credits` = "pc not connected", Grok Bot chip, wallpaper |
| workspaces | digits 1–5 switch (2 = "TV & Inputs" with our TV/HDMI 1–3 windows) |
| **TV tile** | OK → EXIT → back to the stock TV (HDMI2 with signal) |
| **Sources tile** | OK → launcher closes, native **Bronlijst** opens on the current source |
| stock portal | tile **OmarchyTV** (P2b) opens this UI from the remote |
| perf | responsive; no obvious lag in navigation/rendering |

Screenshots: `artifacts/protocol/p3/01-home.png`, `02-workspace2-inputs.png`,
`03-sources-bronlijst.png`, `04-tv-tile-handback.png`.

## Weather location (fixed)

Upstream ships its author's town: `weatherCity: "Alessandria"` (IT) and an "Automatic (from IP)"
entry that — despite the label — only reset the city to Alessandria. On this set the weather tile
showed Alessandria.

Three changes in the fork:

1. **Default city** — `platform.js` seeds the saved config (`localStorage["omarchytv_cfg_2110"]`)
   with `OMARCHY_VESTEL_WEATHER` when the stored city is still a known default (empty, `Alessandria`
   or our earlier guess `Amsterdam`). A city the user chose themselves is never overwritten.
   Now: **Heiloo, NL (52.60252, 4.68815)** — verified on screen: `Heiloo · clear · 14° · min 6° · max 14°`.
2. **"Automatic (from IP)" is real** — it queries `https://ipwho.is/` and sets city + lat/lon
   (explicitly user-triggered; this is the only new third-party call added by the port).
3. **City search works** — the hardcoded Milano/Roma/Berlin/San Francisco list was replaced by real
   geocoding: type a name, pick the `look up "<name>"` entry, and it resolves via
   `geocoding-api.open-meteo.com` (same host family the app already uses for the forecast).

To change the default for this TV: edit `OMARCHY_VESTEL_WEATHER` in
`launcher/omarchy/platform.js` → `/buffer/launcher/omarchy/platform.js`, reload.

## System panel = the TV itself (no laptop)

The `omarchy · pc` panel originally polled a host (upstream's PC link). On this set the laptop can be
away, so it now polls **the TV's own stats**:

* new CGI `launcher/cgi-bin/stats` (busybox shell; there is no Python on the TV) -> JSON
  `{host, kernel, cpu, ram, deploy, temp, uptime, diskConf, diskBuffer, ts}`
* `cpu` = delta between calls against `/proc/stat` (first call after boot falls back to the 1-minute
  load average, clamped); `ram` from `MemTotal`/`MemAvailable`; `host` from `/conf/friendlyname.txt`;
  `temp` from `thermal_zone*` when present
* `app.js` accepts a full URL in `cfg.pcHost` (used verbatim); `platform.js` sets it to
  `http://127.0.0.1:8181/cgi-bin/stats`; the panel label is now **"tv · system"**
* the `ai credits` panel is hidden (`cfg.widgets.ai = false`) - upstream would otherwise show fake
  dollar figures with no AI backend
* `tools/pcstatus.py` stays in the repo as an *optional* alternative (point `OMARCHY_VESTEL_PC` at
  `192.168.178.167:8765`); it is not used by default

Verified on screen: `tv · system -> TELEFUNKEN_70:54:b4:fe:70:c6 · 4.9.118+ / CPU 13% / RAM 52%`,
with the laptop's server stopped.

Gotcha: writing a partial `cfg.widgets` object into localStorage hides **every** panel - always merge
with the defaults (`Object.assign({term,weather,pc,ai,deploy}, saved)`).

## Clock follows the localisation (24 h)

Upstream's `clockFormat: "auto"` means "24 h unless the UI language is English", and this browser
reports `en-US`, so the bar clock showed `2:06 PM` while the set sits in a 24 h country.

Fix: `platform.js` computes `window.OMARCHY_PREFERS_24H` and `use24h()` consults it for `auto`.
The decision is made from the **timezone**, not the UI language:

* this browser reports `Intl.DateTimeFormat().resolvedOptions().timeZone` as a bare **`CET`**
  (not `Europe/Amsterdam`), which is why the first attempt (matching `^Europe/`) still gave 12 h —
  measured with a temporary on-screen debug readout: `tz=CET lt=1:00:00 PM h24=false`
* the check now accepts legacy zone names (`CET/CEST/MET/EET/WET/GMT/UTC`) plus `Europe/`, `Africa/`,
  `Asia/`, `Australia/`; known 12 h zones (`America/New_York`, …) stay 12 h
* fallback: render 13:00 with `toLocaleTimeString()` and look for `am/pm`

Result: `fri 30 oct 14:10`. The app's own **Clock format** setting (auto / 12 / 24) still overrides
this, so it can be changed per user.

Gotcha: this browser caches the scripts aggressively — after changing `app.js` / `platform.js` the
`index.html` script tags must be bumped (`?v=N`), otherwise the old code keeps running.

## Polish (truthful UI)

The set has no Bluetooth audio, no battery, no AI backend and no SpaceX data, so those decorations
were removed rather than left lying:

* `/cgi-bin/net.js` (busybox CGI, `text/javascript`) exports `window.OMARCHY_NET_NAME` =
  `eth0 · 192.168.178.87 up`, loaded *before* `app.js`; the terminal panel now prints that instead of
  the hardcoded `Wi-Fi`
* hidden via the injected stylesheet in `platform.js`: `[data-action=bt]`, `[data-action=batt]`,
  `#spacex-pill` (placeholder countdown), `#grokbot-companion` (no backend)

## Idle behaviour: verified (screensaver + hand-back)

* **Screensaver** works: after `saverIdleMin` without input the app loads `saver/index.html` in an iframe
  — the **Aquarium** (asciiquarium × Second Reality homage) renders on this hardware — and any remote
  key dismisses it (`OmarchySaver.dismiss`). Tested with `?devsaver=0.15` (9 s) and a RIGHT press.
* **"Turn off after idle"** now **hands the screen back** (`EXIT`) instead of forcing standby:
  `<command>Standby</command>` returned 201 but was ignored in several app states (tested from the LAN
  too), while `EXIT` always works — and the set then applies its own no-signal timer (~3 min) and
  suspends, which WoL can undo. Verified end to end: 1 min idle → launcher closed → ~3 min later the TV
  was asleep (`ssh closed`, no ping), then WoL woke it with uptime preserved.
* Dev hooks for testing, gated on the URL: `?devsaver=<min>` / `?devsaver=off` / `?devoff=<min>`.

## Localisation (Dutch)

Upstream ships en/it/de only, so the UI was English and the date read `fri 30 oct`.

* added an **`nl` STRINGS block** (~75 keys: menus, hints, panels, weather conditions) and Dutch
  day/month abbreviations (`zo..za`, `jan..dec` → `vr 30 okt`)
* `platform.js` seeds `cfg.lang = "nl"` when no language was chosen in the app (a deliberate pick is
  kept); the app's language menu can still switch back to English/Italian/German
* with `lang = nl`, upstream's own "auto" clock rule would already choose 24 h, so the timezone-based
  `OMARCHY_PREFERS_24H` is now a second line of defence rather than the only one
* our own tile labels were translated too (`apps & instellingen`)

## Themes, wallpapers and the animated intro

* **All 28 themes** now have their artwork: 53 `.webp` (downscaled to 1920x1080, q82) + the 5 `.mp4`
  intros = **4.6 MB** in `/buffer/launcher/omarchy/wallpapers/` (was 2 themes). Source: upstream
  `launcher/app/src/main/assets/wallpapers/`, fetched from GitLab; 15 of the files hit the API rate limit
  and some are stored **base64** in the repo — both handled by a decode-and-verify fetcher.
* **Animated intro works**: on home resume the app plays `wallpapers/last-horizon/01-eyes-wide.mp4`
  in `#wallpaper-video` (Chrome 67 plays it; verified by comparing two frames ~1 s apart — the
  background changes, the clock does not).
* **Theme switching verified**: picked `lumon` from System › Thema's — palette *and* artwork changed.
* Switching themes stores an explicit wallpaper path, so restoring only `cfg.theme` leaves the old
  image: the `?theme=<id>` test hook also clears `wallpaper` and `wallpaperIdx`.
* **Quirk**: a launched URL with more than one query parameter gets mangled by the app/browser —
  `?theme=last-horizon&fix=1` produced the theme id `last-horizonfix` (unknown theme → no wallpaper).
  Keep launcher URLs to a single parameter.

## Removing the remaining placards

* **System menu**: the rows that need webOS services or an AI backend are filtered out
  (`skipVestel: true` + `window.OMARCHY_HIDE_UNSUPPORTED`): *Cinemarchy*, *Plug-ins (SpaceX TV &
  Grok Bot)*, *Ambilight & Lights*. What remains works on this set.
* **Workspace 4** was upstream's "pair a PC" page. It is now **"Deze tv"**: the string-table entries
  were rewritten (en/it/de/nl) and the *pair* / *simulate connected PC* buttons removed — with null
  guards in `app.js`, because the render and click handlers dereferenced them unconditionally.
  It shows `host · kernel / CPU % · RAM % / up Ns` from `/cgi-bin/stats`.
* GPU placeholder (`18%`) replaced by `–` when the TV reports none.
* The status line no longer says "Connected to … (http://…)" — it just shows the values.

Lesson recorded: **patching the DOM after load is not enough** — the app re-renders those panels from
its string table on every `renderAll()`, so the fix has to live in the table (or in the render code).

## Suspend works with the launcher (resume watcher)

`<command>Standby</command>` is ignored on this build (tested from source, portal and with the page in
front), but the **power key (`1012`) does suspend** the set — including with the launcher in front.

Problem: the page does not survive suspend (the browser drops it), so the TV wakes on its source.
Fix: the hook now runs a **resume watcher** — it clears the kernel ring buffer at boot and, when it sees
`PM: suspend exit`, presses `MENU` (portal up) and re-posts `browserseturl`, retrying until
`/conf/launcher_alive` (the page's 5 s heartbeat) is newer than the resume timestamp.

Measured on the TV: suspend → ASLEEP; WoL → awake; `resume: attempt 1..3 did not stick`,
`resume: launcher back (try 4)`, heartbeat fresh, launcher on screen. Attempts 1-3 are expected: the
portal needs a couple of seconds after resume before it accepts the URL.

System › "Turn off after idle" now calls the power key instead of handing the screen back, so the idle
path keeps the launcher and the watcher restores it. `tools/launcher.sh sleep` does it on demand.

## Known gaps / next

* Tiles are limited to what this TV can do (TV / Sources / stock portal). Streaming apps are
  disabled in `nvt.app.debug.ini`, so there is nothing else to launch yet.
* The stock portal tile's caption still reads "amazon alexa" (see P2b).
* Not wired: PC link (`omarchy · pc`), AI credits, plugins, screensaver, animated intros. The
  screensaver (`tte.js`) is probably too heavy; the animated wallpapers need `video/webm`.
* Status panel says "net Wi-Fi" and shows Wi-Fi/BT glyphs — we are on Ethernet; the upstream
  helper reads webOS services. Cosmetic.
* Only 2 wallpapers are bundled (last-horizon, tokyo-night); other themes fall back to the
  app's procedural background.
* Update path for upstream: we carry a fork with 3 small patches — re-apply on each release
  (diff is ~20 lines in `app.js`, one line in `index.html`, and `platform.js`).
