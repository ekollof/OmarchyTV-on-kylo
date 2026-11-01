# P2 — boot integration + watchdog (2026-10-30)

Goal: the TV boots **into our launcher** when we ask it to, and a broken launcher can never
lock the owner out of the TV. Everything stays in `/conf` (unsigned) and `/buffer`.

## What the hook does now

`/conf/remotelf/aurora.elf` (after sshd/telnetd/DIAL) added two things:

1. **Static server** — `httpd -p 8181 -h /buffer/launcher` (busybox), started when
   `/conf/launcher_enable` exists and `/buffer/launcher` is present.
2. **Opener** — a detached loop that waits for DIAL (`56790/dd.xml`), then asks the app to
   show `http://127.0.0.1:8181/?boot=N` via `browserseturl`, up to **15 attempts**
   (4 s wait + 7 s settle each ≈ 2¾ min), stopping as soon as the page reports success.
3. **Watchdog A (page)** — success = `/conf/launcher_ok`, which the page writes through
   `/cgi-bin/act?a=boot-ok`. On success the failure streak resets; on failure it increments
   `/conf/launcher_fail`, and at **3 consecutive failed boots the flag is deleted** and the
   server is stopped. That is the lock-out guard.
4. **Watchdog B (httpd)** — every 20 s, restart `httpd` if it died while the flag is set;
   when the flag disappears, stop `httpd` again.

Boot sequence: hook → sshd/telnetd/DIAL → httpd → aurora (stock UI) → opener flips the
screen to the launcher once it renders. Seen in the log:

```
launcher: httpd=1940 fail_streak=0
launcher: open attempt 6
launcher: OPENED on attempt 6 (ok Fri Oct 30 13:07:32 CET 2026)
```

(Attempt 6 is normal — the browser only accepts the URL once the app's portal is up.)

## Verified on the TV

| test | result |
|---|---|
| enable + reboot | launcher on screen after ~45 s, success marker written, streak 0 |
| kill `httpd` | respawned in <20 s (`launcher: httpd respawned …`), page still reachable |
| **broken page** (`index.html` moved away, streak pre-seeded to 2) + reboot | 15 attempts, then `FAILED this boot (streak 3/3)` → `DISABLED after 3 failures`, flag removed, httpd stopped, **stock UI intact** |
| restore page + reboot | launcher back (attempt 6), streak 0 |

Screenshot: `artifacts/protocol/p2/01-boot-launcher.png` (status line reads
`boot recorded (hook sees /conf/launcher_ok)`).

## Files / switches

| path | meaning |
|---|---|
| `/conf/launcher_enable` | opt-in flag (**now set** on the TV) |
| `/conf/launcher_ok` | written by the page when it renders; proves the opener worked |
| `/conf/launcher_fail` | consecutive failed boots (0 on success, auto-disable at 3) |
| `/buffer/launcher/` | the web app (`index.html`, `cgi-bin/act`) |
| `tools/launcher.sh open\|close\|status` | host-side control: open the launcher, EXIT back, or report state |

Disable: `rm /conf/launcher_enable` (the watchdog stops httpd within 20 s). Re-enable:
`touch /conf/launcher_enable`.

## Still open (for P3 / P4)

* **Re-entry.** EXIT hands the screen to the stock UI, but nothing brings the launcher back
  except `tools/launcher.sh open` (or a reboot). Options: a stock-portal tile (app-list
  research), a periodic re-open, or Model C.
* **Startup cost.** The launcher only appears once the portal is up (~40-60 s after power-on).
  A native splash on fb1/fb2 before that is a P4 idea.
* **Payload image** (`artifacts/payload/apdat0_full.ubifs`) is stale again — the hook changed
  (P5 rebuild). The live TV already has the new hook.
