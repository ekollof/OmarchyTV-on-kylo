# Full-stack takeover rehearsal — 2026-11-05

Offline rehearsal of the deferred TV full-portal takeover sequence. It uses the
same production module set and order, with the host recorder display and the
host plat fixture instead of fb1/real HAL; no TV or device is touched.

## Sequence

`tests/full_stack_ipc.cjs` starts the real `openaurora` host over a Unix
control socket and loads/starts:

```
plat  nav_display  settings  service_db  image_decode
ir    ui_engine    ui_vendor
```

Config uses the firmware-dump `settingsDB.db` (read-only), the workbench UI
assets, `ui.focus=2`, `ui.live_cards=0`, `ui.paint=1`, `ir.device=` and
`ir.blast=0`. The display is `ui_nav_display.so` (recorder), not `display_fb`;
the plat is the host fixture, not vendor HAL.

It then:

- asserts `ui-state` = `portal screen 0`;
- checks `status ir` publishes `input:v1`;
- feeds `key 22` twice and observes portal repaint;
- `reload ir`, rechecks `input:v1`, feeds `key 21`;
- `reload ui_vendor`, checks the portal republished and feeds `key 22`;
- stops/unloads every module in reverse and verifies clean socket removal/exit.

## Result

```
make -j24 check-full-stack-ipc
ok full-stack IPC rehearsal: plat/settings/service_db/image_decode/ir/ui_engine/ui_vendor, keys and reloads
```

This is the closest offline rehearsal of the TV sequence: the only replacements
are recorder display, host plat and injected key frames.

## Limits

- No physical remote, `/dev/ttyS2`, BLAST `/dev/video28`, fb1, vendor HAL,
  GPU/Mali or cold startup.
- The recorder proves module/service ordering, key routing and socket
  reloads, not TV-side scanout or UART/BLAST acceptance.
- The deferred TV run still requires explicit owner authorization.
