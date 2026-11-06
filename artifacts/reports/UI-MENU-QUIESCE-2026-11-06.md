# ui_menu async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_ui_menu`, the shipped `menus/sub_launcher.json` and a diagnostic
`ui_menu.cleanup_delay_ms`. No display, TV, fbdev or input device was touched.

## State held by ui_menu

`mod_ui_menu` owns the parsed `oa_menu_doc` JSON model (root DOM plus dialog,
branch, item and string views) and publishes `ui.menu`. Consumers hold the
service while using any returned pointer; `document()` returns NULL once the
provider is unpublished.

## Gate

`m_quiesce`:

1. if a worker is already running, `pthread_tryjoin_np` it; return 0 when it
   has finished, `-EAGAIN` while it is busy, `-EIO` on a join error;
2. clear `published` and unpublish `ui.menu`, so `document()`/`dialog()`/
   `branch()` return NULL and new lookups get `ENODEV`;
3. detach the document (`c->doc = NULL`);
4. with `ui_menu.cleanup_delay_ms=0`, `oa_menu_free(doc)` synchronously;
5. otherwise free it on a worker after the diagnostic delay and return
   `-EAGAIN`.

The worker makes no host API calls; it only sleeps and frees the detached
document. `m_stop` joins a leftover worker and frees any document that was not
detached; `m_destroy` joins/frees too. The gate is idempotent for the host's
stop-then-unload sequence. `m_start` refuses with `-EBUSY` if a document,
publication or cleanup worker is still live.

## Control probe

New diagnostic command:

```text
menu-state <dialog>     query ui.menu dialogs/branches
```

It holds `ui.menu`, reads `document()->dialog_count` and the dialog's
`branch_count`, then releases. A missing provider returns `ENODEV`; a missing
dialog returns `ENOENT`.

## Verification

`check-ui-menu-quiesce` starts the host with `ui_menu.cleanup_delay_ms=400`:

```text
load/start ui_menu
menu-state sub_launcher -> menu sub_launcher dialogs 1 branches 5
reload ui_menu -> failed (fast; first elapsed 0 ms in this run)
status ui_menu -> responsive (< 1 s)
menu-state sub_launcher -> failed errno 19      (service unpublished)
wait 600 ms
reload ui_menu -> ok
menu-state sub_launcher -> menu sub_launcher dialogs 1 branches 5
stop ui_menu -> failed; menu-state -> failed errno 19
wait 600 ms; stop ui_menu -> ok; unload ui_menu -> ok; quit -> bye
```

```text
ok ui_menu quiesce: reload fails fast (0ms), document republished, retry completes
```

The second `menu-state` proves the reloaded document is republished, not just
that the process survived. The existing `ui.menu service` test still covers the
JSON tree, failed-load isolation and unload.

The ARM module now links `libpthread`:

```text
build-tv-v3/modules/mod_ui_menu.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The gate frees the document on a worker; it does not prove a live display
  consumer's teardown or a vendor-UI menu handoff.
- Consumers must keep the host's hold discipline and must not retain a menu
  pointer across provider stop/unload. `ui_vendor` looks up/releases per use;
  a future long-lived holder needs its own handoff.
- This is the JSON menu model, not the running vendor `SubLauncherDialog` or
  its in-memory `SettingItem` vectors.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source` and
  now `ui_menu` have the async gate; the remaining modules still stop
  synchronously.
- No TV or stock-Aurora coexistence test was performed.
