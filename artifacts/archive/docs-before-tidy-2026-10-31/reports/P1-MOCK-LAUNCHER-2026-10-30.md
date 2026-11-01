# P1 — mock launcher, end-to-end on the TV (2026-10-30)

Goal: prove the Model A loop with *our own* page — launcher → stock TV → launcher — and that
tiles can drive the TV. Built as a throw-away mock before porting OmarchyTV.

## What was built

```
launcher/index.html     3 tiles (TV / Sources / Apps), ES5, arrow keys + Enter, clock
launcher/cgi-bin/act    busybox-httpd CGI -> DIAL key POSTs on 127.0.0.1:56789
```

Deployed to the TV as `/buffer/launcher` (UBIFS, persistent, 209 MB free) and served by the
TV's own busybox **`httpd -p 8181 -h /buffer/launcher`**. The page is opened with a plain
`browserseturl` to `http://127.0.0.1:8181/`, so everything is same-origin and the CGI does the
DIAL calls (page JS cannot POST to `:56789` — CORS, see P0).

| tile | CGI does | result on the TV |
|---|---|---|
| **TV** | `1037` (EXIT) | page closes, stock UI/current source takes over |
| **Sources** | `1037`, 1 s, `1056` | page closes, native **Bronlijst** opens on the current source |
| **Apps** | `1037`, 1 s, `1048` | page closes, stock portal ("Startpagina") opens |

Screenshots: `artifacts/protocol/p1/01-launcher.png`, `02-sources-bronlijst.png`,
`03-hdmi2-signal.png`, `04-apps-stock-portal.png`.

Full loop verified: `browserseturl` → launcher renders (fb0) → arrows move focus → OK runs the
tile → stock UI → `browserseturl` again → launcher. Repeated several times.

## Findings that matter for the real port

1. **The browser eats "TV-control" keys.** With the page in front, `1056` does not open the
   Bronlijst — the TV closes the page instead. That is why the CGI sends `EXIT` first and the
   source key one second later. Same for MENU/portal.
2. **The Bronlijst is transient.** It times out in ~2 s. Navigate in one tight burst
   (~0.25 s between keys); a screenshot in the middle kills the window and the arrows land in
   the channel list. For a user-driven flow this is irrelevant (they press the remote), but any
   "one-press source" feature must be a single burst.
3. **`fb0` is BGRA, `fb1` is RGBA.** `tools/tvshot.py` now swaps red/blue on layer 0 by default
   (`--format` to override). Before the fix the launcher looked orange in captures.
4. **CGI needs the headers**: busybox httpd returns "error getting response" if a CGI writes a
   body without `Content-Type` + blank line. (Cost me one debugging round; noted in the script.)
5. **Viewport is 1280x720 CSS** (DPR 1.5): the mock is laid out for that, and it fills the panel.

## State left on the TV

- `/buffer/launcher` present; `httpd` running on **0.0.0.0:8181** (stop with
  `pkill -f 'httpd -p 8181'`; it does not survive a reboot).
- TV is on **HDMI2** with a live signal (the owner's laptop, DP-1 of the host, `dpmsStatus 1`).
- Nothing in `/conf` changed, no boot integration yet (that is P2).

## Next (P2)

- Move the flag into the hook behind `/conf/launcher_enable` (default **off**) +
  supervisor that opens the launcher once per boot via the CGI/DIAL path and disables itself
  after repeated failures.
- Decide the re-entry route from stock TV (MENU → portal is reachable; the launcher needs a
  trigger: DIAL from LAN, or a stock-portal tile if we can register one).
