# ui_omarchy skin socket lifecycle — 2026-11-05

Host-only test using the recorder display. No TV/HAL/framebuffer module.

## What was wired

`tests/skin_socket.sh` already covered real Unix-socket `skin` selection but
was not in `make check`. It is now staged and run by `check-ui-skin-socket`
with `mod_nav_display`, `ui_engine`, `ui_vendor` and `ui_omarchy`.

The test starts the host with a deliberately missing/`../invalid` configured
skin and verifies:

- startup falls back to `vendor` with `fallback=1` and the expected errno;
- `skin omarchy` loads/starts `ui_omarchy` and reaches `active omarchy
  state ready fallback 0`;
- `skin vendor` switches back and reaches `active vendor state ready`;
- `skin badenter` records the enter failure and stays on vendor with
  `fallback 1 error 5`;
- `quit` shuts the host down cleanly.

Result:

```
ok Unix socket startup fallback, runtime switching, enter failure and clean process shutdown
```

## Evidence

- `artifacts/analysis/skin-socket-2026-11-05/check.txt`
- `full-host-check.txt` (399 ok)
- `identities.txt` (host, ui_engine, ui_vendor, ui_omarchy, recorder display)
- `verify.cjs`: 45 groups

This extends real socket/CLI coverage to the Omarchy skin and runtime skin
selection/fallback. It remains recorder-display/host evidence, not physical
fb1 ownership, hardware-module reload or cold startup.
