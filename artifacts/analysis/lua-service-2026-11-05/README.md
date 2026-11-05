# Lua service evidence

Interpretation: [LUA-SERVICE](../../reports/LUA-SERVICE-2026-11-05.md).

- `check.txt`: full host `make check` log (433 ok).
- `verify.txt`: `verify.cjs` log (45 groups).
- `check-ui-lua.txt`, `check-ui-skin.txt`: focused host logs.
- `check-lua-ipc.txt`: real Unix-socket load/start/reload candidate test.
- `sanitizer.txt`: leak-enabled ASan/UBSan `check-ui-lua` run.
- `arm-qemu.txt`: ARM glibc `tests/ui_lua` under qemu-arm-static.
- `identities.txt`: source/module/test and sanitizer/ARM binary hashes
  (paths relative to `openaurora/`).

No TV or hardware was accessed. The host HTTP success body is not exercised
under qemu-user; that run takes the failed-transfer callback.
