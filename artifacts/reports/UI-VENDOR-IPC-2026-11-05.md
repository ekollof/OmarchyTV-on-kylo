# ui_vendor / recorder-display socket lifecycle — 2026-11-05

Host-only test. No display, HAL, framebuffer or TV module is loaded.

## What was added

- `ui-state` control command: looks up `ui.portal` v1 and prints
  `portal screen <n>` (or `ENODEV` when the skin is absent).
- `tests/ui_vendor_ipc.cjs`: starts a real `openaurora --sock` host with
  `mod_nav_display` (the same display/recorder test provider used by the UI
  tests), `ui_engine`, and `ui_vendor`. It drives load/start/use/stop/reload/
  unload and the recorder display provider reload through the real Unix
  socket; `status ui_vendor` is also checked through stdin.

## Verified sequence

```
ui-state before load                 failed errno 19 (ENODEV)
load/start nav_display               ok
load/start ui_engine                 ok
load/start ui_vendor                 ok
ui-state                             portal screen 0
status ui_vendor (stdin)             name ui_vendor / state started /
                                     ui.portal:v1
stop ui_vendor                       ok
ui-state                             failed errno 19
start ui_vendor                      ok
ui-state                             portal screen 0
reload ui_vendor                     ok
ui-state                             portal screen 0
stop ui_vendor + ui_engine           ok
reload nav_display                   ok
start ui_engine + ui_vendor          ok
ui-state                             portal screen 0
stop + unload ui_vendor              ok
load + start ui_vendor               ok
ui-state                             portal screen 0
SIGTERM                              exit 0, socket removed
```

Result:

```
ok actual ui_vendor/display Unix IPC: start/use/stop/reload/display-provider reload/unload lifecycle
```

## Evidence

- `artifacts/analysis/ui-vendor-ipc-2026-11-05/check.txt`
- `full-host-check.txt` (396 ok)
- `identities.txt` (host, ui_vendor, ui_engine, recorder display DSO hashes)
- `verify.cjs`: 45 groups

This closes the real socket/CLI reload milestone for `ui_vendor` and the
recorder display provider. It does not prove physical fb1 ownership, cold
startup, stock visual parity, or a full vendor replace/restore cycle.
