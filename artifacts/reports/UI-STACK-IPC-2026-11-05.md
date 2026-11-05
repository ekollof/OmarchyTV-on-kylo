# Non-HAL UI stack socket/reload — 2026-11-05

Host-only test. No display, HAL, framebuffer or TV module is loaded.

## Stack

`tests/ui_stack_ipc.cjs` starts a real `openaurora --sock` host with:

- `settings` on a copied read-only `settingsDB.db`,
- `image_decode`,
- `mod_nav_display` (recorder display),
- `ui_engine`,
- `ui_vendor`.

It drives the stack over the Unix socket and stdin. Module dependencies are
real: settings provides `launcher.db`, nav_display provides `display`,
ui_engine consumes it, ui_vendor consumes `ui.engine`.

## Verified sequence

```
launcher-items 0 before settings      failed errno 19 (ENODEV)
load/start settings                  ok; launcher-items 0 => count 23
load/start image_decode              ok; image-decode thumb61 =>
                                     image 400x225 fnv 12815d59be310b91
load/start nav_display               ok
load/start ui_engine                 ok
load/start ui_vendor                 ok; ui-state => portal screen 0
status settings via stdin            name settings / state started
reload image_decode                  ok; decode hash unchanged
stop ui_vendor; reload settings      ok; launcher rows still returned
start ui_vendor                      ok; portal screen 0
stop ui_vendor; reload ui_engine     ok; start ui_vendor; portal screen 0
stop ui_vendor + ui_engine
reload nav_display                   ok; restart engine+vendor; portal screen 0
stop + unload ui_vendor              ok; load/start; portal screen 0
SIGTERM                              exit 0, socket removed
```

Result:

```
ok actual non-HAL UI stack IPC: settings/launcher rows, image.decode, recorder display and ui_vendor reloads
```

## Evidence

- `artifacts/analysis/ui-stack-ipc-2026-11-05/check.txt`
- `full-host-check.txt` (397 ok)
- `identities.txt` (host, settings, image_decode, ui_engine, ui_vendor,
  recorder display)
- `verify.cjs`: 45 groups

This covers a real multi-module dependency stack and reloads of its service
providers. It is not the full hardware module set, physical fb1 ownership,
cold startup or stock parity.
