# ui_engine async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real engine,
the host display recorder and a real OAI1 asset. No TV/fbdev/paint or stock
process was touched.

## State held by ui_engine

`ui_engine` owns the display/alpha/async service holds, the OAI1 icon cache,
the skin registration and dispatch queue, the key subscription and the repaint
timer. Its old quiesce only set `draining` and refused while a skin dispatch was
on the stack; it did not tear down.

## Gate

`m_quiesce` now:

1. returns `-EAGAIN` without touching state while `dispatching` is set (a skin
   callback is on the stack);
2. otherwise tears down synchronously: unpublish `ui.skin`/`ui.engine`, drop the
   display/alpha/async holds, unsubscribe module/service/key events, delete the
   repaint and rebind timers, and clear the skin/dispatch/event state;
3. detaches the cached icon RGBA pointers and sets `n=0`;
4. frees them synchronously, or hands them to a worker when
   `ui_engine.cleanup_delay_ms` is set and returns `-EAGAIN`.

The worker only sleeps/free and makes no host calls. `m_destroy` joins a
leftover worker.

The gate is idempotent: a second call after the icon cache is empty returns 0
without another delay, as reload/unload require.

## Control probe

New diagnostic command:

```text
ui-icon <name>     load/report a ui.engine OAI1 asset
```

It returns `icon <name> <w>x<h>` and exercises the same lazy cache the vendor
skin uses. The test loads `settings_top` (1920x180) before reload, so the
detached free is real.

## Verification

```text
ok ui_engine quiesce: reload fails fast, asset cache reloads, retry completes
```

Sequence with `ui_engine.cleanup_delay_ms=400`:

```text
load/start nav_display, ui_engine
ui-icon settings_top -> icon settings_top 1920x180
reload ui_engine -> failed (<1.5 s)
status ui_engine -> responsive (<1 s)
retry reload -> ok
ui-icon settings_top -> icon settings_top 1920x180
stop ui_engine -> failed; retry -> ok; unload -> ok
```

The second `ui-icon` proves the engine services and asset cache are live after
the worker completes. Full suite and QEMU:

```text
full_check=0
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
```

## Limits

- `ir`, `i18n`, `settings`, `ui_vendor` and now `ui_engine` have the async gate;
  later gates add display, source and ui_menu.
- The test uses the host recorder, not a GPU/scanout or Mali teardown. A live
  display close during scanout is still unproven.
- No TV reload or stock-Aurora coexistence was tested.
