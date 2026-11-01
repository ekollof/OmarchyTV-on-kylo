# P0 — browser / key / switch probe results (2026-10-30, on the TV)

> **Status: Historical web feasibility measurement. Not current deployment state or native hardware acceptance.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Goal: find out whether an OmarchyTV-style **web** launcher can work on this set, and how
we would switch between it and stock aurora. Nothing was installed; a temporary page was
served (`/tmp/probe` on the TV + the laptop on `:8010`), opened with DIAL, then removed.

## 1. The browser is Chromium 67 — good news

Full UA (truncated at 240 chars in the beacon; the rest is `SmartTvA/3.0.0`):

```
Mozilla/5.0 (Linux) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/67.0.3396.99
Safari/537.36 OPR/46.0.2207.0 OMI/4.13.5.431.SIERRA.170 Model/Vestel-MB230 VSTVB MB200
HbbTV/1.5.1 (+DRM; TELEFUNKEN; MB230; 0.51.52.3; ; _TV_NT72671_2019;)
```

| property | value |
|---|---|
| engine | **Chrome 67** (Opera 46 / OMI 4.13.5.431) |
| screen | 1920×1080, **devicePixelRatio 1.5** → **CSS viewport 1280×720** |
| features | **24 / 28** of the probe set |
| missing | `CSS aspect-ratio`, `CSS inset`, `CSS :is()`, `touch-events` |

That is *well* above OmarchyTV's own floor (its `compat.js` targets Chrome 38), and the two
missing CSS features are exactly what their compat layer emulates. The old engine is not a
blocker.

Layout consequence: the launcher is a **1280×720 CSS canvas on a 1080p panel** — size UI in
CSS px, not 1080p px (their webOS build already does this).

## 2. Keys arrive as normal DOM events

| remote key | DIAL code | DOM event |
|---|---|---|
| Up / Down / Left / Right | 1020 / 1019 / 1021 / 1022 | `ArrowUp` / `ArrowDown` / `ArrowLeft` / `ArrowRight` (keyCode 38/40/37/39) |
| OK | 1053 | `Enter` (13) |
| digits | 1000–1009 | `DigitN` (keyCode 48–57) |
| — | — | MENU (1048) and EXIT (1037) are **consumed by the TV**, never reach the page |

`key`/`code`/`keyCode` are all present, so OmarchyTV's `LEGACY_KEYS` path is a no-op here.

## 3. Same-origin CGI works, cross-origin DIAL does not

* Page served from `http://127.0.0.1:8181/` (busybox `httpd` on the TV, **temporary**):
  `fetch('/cgi-bin/test')` → **200 + body**. So a busybox-httpd + shell-CGI helper is fine.
* `fetch('http://127.0.0.1:56789/apps/SmartCenter', {method:'POST', …})` → **`TypeError`**
  (CORS preflight). The page cannot talk to DIAL directly.
  → **Design confirmed**: the page calls its own CGI, the CGI does the DIAL `wget -qO- --post-data`.

Also note: the "launcher page is blank" traps we hit are worth remembering —
*framebuffer content can be stale*. After EXIT, `fb0` still showed our page while the page
had already been closed (no key events). **Pixels are not proof; beacons are.** Check
`ps | grep omi_worker` or send a key and look for a beacon.

## 4. Switching works exactly as Model A hoped

| step | result |
|---|---|
| `browserseturl` → page | page loads, receives keys (`ev=load`, `ev=key`) |
| EXIT (1037) | page **stops receiving keys** → browser closed, TV returns to **stock TV UI** (`C12`, channel banner) |
| `browserseturl` again | page live again — repeatable, no leftovers |
| MENU (1048) in TV mode | opens the **stock portal** ("Startpagina" with the app tiles) |
| SOURCE-ish key 1056 | TV switches to the tuner/info banner; page stops receiving keys |

So: **EXIT = back to stock, one DIAL call = back into the launcher.** No process games
needed. `MODE A` is viable and cheap.

## 5. Sources / inputs — SOLVED (P0b, same day)

`KeyAux` (**1056**) opens the native **"Bronlijst"** OSD. Real list on this set
(9 rows, `UP`/`DOWN` **wrap around**, it opens with the *current* source highlighted):

```
1. TV          2. BACK AV     3. HDMI1   4. HDMI2   5. HDMI3
6. YPbPr       7. VGA/PC      8. Wireless Display        Audio video delen
```

Confirmed end-to-end: `1056, DOWN, DOWN, DOWN, OK` selected **HDMI2** and the OSD banner
read `HDMI2 | HDMI` (i.e. a live HDMI signal, no "Geen signaal"). While HDMI2 is selected
and its source sleeps, the TV shows the no-signal banner — a usable signal oracle.

Caveats found:
* The list is **transient** and the OSD repaints asynchronously — keys must be sent as a
  tight burst (~0.25 s apart); screenshots between keys make it fall out of the list.
* Because it wraps, there is no "clamp at top" anchor. Two viable designs for a
  `Sources` tile: (a) **just send 1056** and let the user navigate the native list with the
  remote (simplest, no state tracking), or (b) compute `(target - current + 9) mod 9`
  moves and track the current source in the launcher.
* Earlier attempts failed because the list had already timed out and the arrows went to the
  channel list instead — always verify with a screenshot right after `1056`.

The `m_source_names` BLOB in `settingsDB` confirms the capability list (TV, BACK AV,
EXT1-S, SIDE AV, S-VIDEO, HDMI1-4, YPbPr, VGA/PC, DVD, Wireless Display, APM).

`aurora.elf` has no SmartCenter command for this (the table at `0x151578c` has
`setcurrentchannel` but no `setsource`); the FROST/test-tool command list (at `0x14e177b`)
does have `GETSOURCE`, `GETHDMISTATUS` and even `get_screen_capture`, but we have not found
a socket for it yet — and we do not need it: **the key sequence is enough**.

## 5b. Earlier notes (kept for the record)

What we learned about the set's inputs so far:

* Sidebar of the stock portal: *home*, *channels* (Zenders: C12, C07, BBC2, … analog cable
  list), *settings*, *smart center*, *last channel* — **no input list in the portal**.
* `KeyAux` (1056) does **not** open a source list; it shows the tuner info banner.
* `m_current_source` in `settingsDB` reads `0` and did not change for any key we tried; the
  DB is not written live (same behaviour as `m_volume`), so it is not a state oracle.
* `aurora.elf` has no SmartCenter command for sources: the command table at
  `0x151578c` is `getwebsocketlink, activechannellist, timerlist, browserseturl, addrecord,
  addtimer, deleterecord, deletetimer, setcurrentchannel, tvstate, specialportalapps,
  setdimmode, mouseevent, setvolume, changechannelinfo, getportalurl, startfollowtv, …` —
  no `setsource`. The ICE layer does have `ice_IsCurrentSourceHDMI/VGA`, `setSource`.
* So: switching inputs is a **UI-only path** (menu walk) until we find the key/menu, and the
  launcher will need either (a) that key/menu sequence replayed by a CGI, or (b) an aurora
  patch (`/vendor` overlay) to call `setSource` directly.

## 6. Power / standby

With the probe page in front, the TV stayed awake for **> 4 minutes** (heartbeat beacons
every 20 s, ssh and DIAL still up) — the ~3 min no-signal standby did **not** fire.
Consequence: a permanently-open launcher page keeps the panel on. The launcher needs
either an explicit screensaver/power-off action or a "release the screen" behaviour
(e.g. EXIT back to TV mode) so the TV can sleep normally.

## 7. Conclusions for the plan

1. `MODE A` (web launcher on top of aurora) is confirmed technically: Chrome 67, arrows/OK/
   digits as DOM events, same-origin CGI, EXIT returns to stock, re-entry via DIAL.
2. OmarchyTV's webOS build fits; its compat layer is more than enough. Port scope can be
   decided after P1 (mock launcher) — nothing in P0 says we must cut features, but the SoC
   is weak, so measure fps with the real app before committing.
3. The two real gaps are **input switching** (§5) and **keeping the TV awake** (§6).
4. Artifacts: `artifacts/protocol/p0/*.png` (probe page on fb0, stock portal, settings,
   channel list, TV state after EXIT).

## 8. Immediate next steps

* P1 mock launcher (3 tiles + one CGI) to prove the loop end-to-end from our own page.
* Find the input/source menu (menu walk + screenshots, or Ghidra on the ICE source calls) —
  required before any "sources" tile is promised.
* Decide what "TV tile" means: EXIT (stock UI) is the honest default.
