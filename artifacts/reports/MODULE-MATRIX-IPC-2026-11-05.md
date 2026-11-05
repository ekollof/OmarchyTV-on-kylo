# Hardware-free module matrix socket/reload — 2026-11-05

Host-only test. No display, HAL, framebuffer or TV module is loaded.

## Matrix

`tests/module_matrix_ipc.cjs` starts a real `openaurora --sock` host with:

- `ticker` → clock service,
- `ui_clock` → requires ticker/clock,
- `ui_anim` → requires ui_clock,
- `ui_navigation`,
- `ui_list`,
- `power`,
- `cec`.

All modules load/start through the socket. `nav-state` and `list-state` are
the real service consumers. The test then reloads independent modules and
performs dependency-ordered clock reloads:

1. reload `ui_navigation`, `ui_list`, `ui_anim`;
2. stop `ui_anim`/`ui_clock`, reload `ticker`, restart `ui_clock`/`ui_anim`;
3. stop `ui_anim`, reload `ui_clock`, restart `ui_anim`;
4. reload `power`;
5. reload/stop/unload/load/start `cec`, retrying its asynchronous cleanup
   window (`cleanup pending; retry stop/unload`).

## Verification

```
ok actual hardware-free module matrix IPC: clock chain, navigation/list, power/cec reloads
```

- Full strict host `make check`: **398 ok**.
- `verify.cjs`: **45 groups**.
- `artifacts/analysis/module-matrix-ipc-2026-11-05/check.txt`,
  `full-host-check.txt`, `identities.txt`.

This broadens real socket/CLI reload coverage beyond image.decode and the UI
stack. It is still not the full hardware module set (plat/display/panel/ir/
audio/audio source/graphics/scene display), physical fb1 ownership, cold
startup or stock parity.
