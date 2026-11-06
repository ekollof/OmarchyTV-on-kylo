# settings async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_settings`,
the shipped settingsDB copied to a temporary file, and the real picture blob.
No TV, fbdev, input device or stock process was touched.

## Why this module

`mod_settings` is a high-state module: it holds a SQLite handle, the 20×0x240
picture blob, and publishes four services (`settings`, `settings.hotel`,
`launcher.db`, `picture.settings`). A locked or slow filesystem could otherwise
stall the control loop during `stop`/`reload`.

## Gate

`m_quiesce` follows the ABI 3 pattern from `mod_ir`/`mod_i18n`:

1. detach `c->db` first and clear every `*_published` flag, so synchronous
   `service/removed` callbacks see a closed provider (`ENODEV`/`-1`) rather
   than a live database;
2. unpublish `picture.settings`, `launcher.db`, `settings.hotel` and
   `settings`;
3. with `settings.cleanup_delay_ms=0` close synchronously (normal case);
4. with a diagnostic delay, hand the SQLite handle to a worker and return
   `-EAGAIN`; the host keeps the module busy and retries stop/reload until the
   worker exits.

The worker calls `sqlite3_close_v2` and makes no host API calls. `m_destroy`
joins the worker if the host unloads after a failed retry.

## Verification

`make check-settings-quiesce` spawns `openaurora`, loads `settings` from a temp
module dir and connects to the real Unix control socket:

```text
picture-state 1 -> picture source 1 mode 1 zoom 1 energy 3
reload settings -> failed (fast, <1.5 s)
status settings -> responsive (<1 s)
(repeat after 600 ms) reload settings -> ok
picture-state 1 -> picture source 1 mode 1 zoom 1 energy 3
stop settings -> failed; retry -> ok; unload -> ok
```

```text
ok settings quiesce: reload fails fast, picture service republished, retry completes
```

The repeated `picture-state 1` proves the service is republished from the same
persisted blob after the reload, not just that the process stayed alive.

The existing settings hotel/lifecycle test also passes, including its
synchronous `service/removed` callback assertions:

```text
ok settings.hotel: readonly companion, SQL integer narrowing/NULL/text, reload and non-waiting busy error
ok settings.hotel: either consumer hold pins provider; removal refuses cached reads; both publication collisions preserve peers
```

Full suite:

```text
full_check=0
```

## Limits

- Only `ir`, `i18n` and now `settings` have the asynchronous gate; the other
  modules still stop synchronously.
- The diagnostic delay exercises the gate; the production path is a fast
  synchronous `sqlite3_close_v2`.
- This is a control-socket lifecycle test, not a live TV reload and not a
  stock-Aurora coexistence test. Writing the TV's real DB still requires
  `settings.readonly=0`.
