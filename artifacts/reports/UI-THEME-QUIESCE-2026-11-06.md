# mod_ui_theme async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_ui_theme`, a synthetic BinSVG index/binary and a diagnostic
`ui_theme.cleanup_delay_ms`. No GPU, renderer, skin or TV was touched.

## Inventory before this change

35 module sources export a descriptor. 17 had a `.quiesce` hook
(`ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
`ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg`, `pvr`, `channels`,
`service_db`, `image_decode`), plus `graphics`, `scene_display` and `lua`
already had one. The remaining synchronous modules were
`audio_amp_profile`, `audio_curve`, `panel`, `plat`, `power`, `ticker`,
`ui_anim`, `ui_clock`, `ui_console`, `ui_diag`, `ui_list`, `ui_navigation`,
`ui_omarchy`, `ui_profile` and `ui_theme`.

`ui_theme` was the highest-state remaining one with a real heap object: the
parsed `oa_svg_pack` (702 records / 11,888 elements in the real corpus). The
module publishes `ui.theme` and callers borrow its immutable views.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `ui.theme` synchronously, so `record`,
   `named` and `count` reject with `ENODEV`;
3. detaches the parsed pack;
4. frees it synchronously at `ui_theme.cleanup_delay_ms=0`, otherwise on a
   worker after the diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls. `m_stop` joins and frees current/detached
packs; `m_destroy` does the same. `m_start` refuses with `-EBUSY` if published,
a worker is live or a pack is still attached, then loads the configured files.

## Control probe

New diagnostic command:

```text
theme-state     query ui.theme pack count
```

It holds `ui.theme`, reads `count()`, then releases. Missing provider returns
`ENODEV`.

## Verification

`check-ui-theme-quiesce` starts with a one-record fixture and
`ui_theme.cleanup_delay_ms=400`:

```text
theme-state -> theme count 1
reload ui_theme -> failed (fast; 1 ms in the full run)
status ui_theme -> responsive (< 1 s)
theme-state -> failed errno 19        (service unpublished)
rewrite fixture to 2 records
wait 600 ms
reload ui_theme -> ok
theme-state -> theme count 2          (replacement loaded the changed pack)
stop ui_theme -> failed; theme-state -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_theme quiesce: reload fails fast (1ms), pack detached, retry completes
```

Existing theme/skin behavior still passes:

```text
  ok   skin discovers late theme, pins provider, drops forced removal, preserves active mode and rebinds
  ok   all 8 modes, numeric priority over stream order, missing resource/markers and explicit overrides
  ok   suspended replacement defers I/O/rebind; resume reacquires; failed skin start releases theme and callback without removing peer
  ok   CLI reload replaces snapshot; edits cannot mutate held views, copied geometry survives reload
  ok   consumer hold pins views and blocks normal stop/unload/reload; publication collision preserves peer and retries
  ok   standby retains snapshot; suspended reload defers load/publication and failed resume activation retries
  ok   missing/special paths, numeric overflow, incomplete/trailing index, NUL and malformed records reject; no stale publication
  ok   invalid later record rejects whole pack; duplicate CRC first wins; owned snapshot survives input deletion; failed load output preserved
  ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse
ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_ui_theme.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok ui_theme quiesce: reload fails fast (1ms), pack detached, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The test uses a tiny synthetic BinSVG pack for the control-socket gate; the
  real 702-record/11,888-element corpus is still covered by `check-ui-theme`,
  but not through this delayed reload.
- Borrowed `oa_binsvg` views remain valid only while the provider is held. The
  host hold check prevents provider stop/reload while a consumer holds it; a
  consumer that retains a view after release is outside the contract.
- The worker frees the pack, but the test observes the gate through
  unpublish/fail-fast/retry and the changed record count, not freed memory.
- No renderer, skin composition or display path is exercised here.
- Remaining synchronous modules: `audio_amp_profile`, `audio_curve`, `panel`,
  `plat`, `power`, `ticker`, `ui_anim`, `ui_clock`, `ui_console`, `ui_diag`,
  `ui_list`, `ui_navigation`, `ui_omarchy`, `ui_profile`.
- No TV or stock-Aurora coexistence test was performed.
