# mod_ui_console async quiesce — 2026-11-06

Offline host control-socket and focused ASan/UBSan tests with the real
`mod_ui_console`, followed by a staged CPU-only ARM run on the TV. No display,
input, audio, tuner or hardware module was loaded on the TV.

## Ownership and gate

`ui_console` is the trivial text UI module: it owns KEY and TICK subscriptions,
the selected tile index and a redraw counter. It publishes no service, so the
fail-fast observable is that its subscriptions are gone and key events during
the gate reach nothing; the selection remains readable through the module
`save()` path.

Quiesce now:

1. fails fast with `-EAGAIN` while a cleanup is pending;
2. clears `active` first so stale or racing callbacks cannot draw or mutate;
3. unsubscribes KEY and TICK synchronously;
4. snapshots the selection and detaches the heap `console_state`;
5. frees it synchronously at delay 0, else from one nonblocking `sched` slice
   after `ui_console.cleanup_delay_ms`.

`save()` falls back to the snapshot after detach, so the host handoff still
works while gated. `m_start` refuses to start while a cleanup is pending,
recreates the state after stop/start, and re-applies the saved selection.
`restore()` clamps an out-of-range selection and materializes state.
`cancel_cleanup()` cancels a queued task and frees an unconsumed snapshot on
stop/destroy. A failed `sched` submit frees the snapshot and leaves the
snapshot fields usable for the retry; the host treats any nonzero return as
pending.

Two project-diagnostic control probes were added:

```text
console-state           read the saved selection via module save()
emit-key <id>           emit one OA_KEY_PRESS on the host KEY topic
```

`emit-key` bypasses IR framing and is only a test hook; `key <code>` remains
the NEC-frame path through `input`. Neither is a module service contract.

## Verification

`check-ui-console-quiesce` starts the module with
`ui_console.cleanup_delay_ms=400` and drives the real control socket:

```text
console-state -> console selection 0
emit-key 160 -> ok; console-state -> console selection 1
reload ui_console -> failed             (fast; 0 ms in the final run)
status ui_console -> responsive (< 1 s), cleanup_pending 1
console-state -> console selection 1    (snapshot survives detach)
emit-key 250 -> ok; console-state -> console selection 1
                                        (no live subscription during the gate)
wait 600 ms
reload ui_console -> ok
console-state -> console selection 1    (handoff preserved)
emit-key 250 -> ok; console-state -> console selection 2
stop ui_console -> failed; console-state -> console selection 2
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok ui_console quiesce: reload fails fast (0ms), selection handed off, retry completes
```

A focused ASan+UBSan build (`build-asan`, `-fsanitize=address,undefined`,
`detect_leaks=1`, `halt_on_error=1`) ran the same test clean:

```text
ok ui_console quiesce: reload fails fast (1ms), selection handed off, retry completes
```

The ARM module links only libc:

```text
build-tv-v3/modules/mod_ui_console.so
  NEEDED libc.so.6
```

Full suite ran visibly and ended `all tests passed` (316 `ok` lines, rc 0).
QEMU oracle checks still pass after the change:

```text
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03; key 20 timer card bottom 405 -> 450 over 28 frames
ok qemu sub-launcher: static AE=0; 300 ms card2 701,276 235x130 -> 701,276 317x175
```

## On-TV ARM CPU-only gate — 2026-11-06

Staging was under `/buffer/oa-console-verify` (writable 232 MB ubi volume), not
`/tmp`; the TV was reached through `tools/tvssh.sh`. Upload and execution were
separate connections.

Hash-verified ARM inputs (remote hashes match the local build):

```text
openaurora         sha256 eddaf4786ee6b048be4b0d13fc36e8a7eecf6c1708bc241e1a3f38035050111b
mod_ui_console.so  sha256 7651a350cbd23edfaede8cee8d1a1ceba676aeace084333676a0c857cd1e1f75
```

Config used `ui_console.cleanup_delay_ms=400`. Sequence/output:

```text
console selection 0
emit-key 160 -> ok; console selection 1
reload ui_console -> failed
console-state -> console selection 1
emit-key 250 -> ok; console-state -> console selection 1   (ignored during gate)
status ui_console -> state started, cleanup_pending 1, otherwise responsive
wait 0.8 s
reload ui_console -> ok; restored 1 bytes; ui_console up (selection 1)
console-state -> console selection 1
emit-key 250 -> ok; console-state -> console selection 2
stop ui_console -> failed; console-state -> console selection 2
wait 0.8 s
stop ui_console -> ok; unload ui_console -> ok; quit -> bye
```

Afterward `/buffer/oa-console-verify` was removed, no `openaurora` helper
remained, and stock Aurora was still PID 1781. Only the staged DSOs, a Unix
control socket and stdin were opened; no `/dev/fb*`, `/dev/ntk*`, `/dev/dvb`,
`/dev/snd`, `/dev/i2c*`, fb1, scanout, input or tuner/HAL path was opened. This
is a CPU-only ARM/glibc scheduler-gate check, not display/HAL, scanout, real
key input or broad UI acceptance.

## Limits

- `ui_console` is the text stand-in, not a renderer. Subscription suppression
  and the saved selection are the only fail-fast observables; no scanout,
  compositor or real key source was involved.
- The test uses a 400 ms cleanup delay and short waits; longer perturbation and
  queued-task cancellation timing are not measured.
- `console-state` and `emit-key` are project-diagnostic control paths, not new
  public services. `emit-key` bypasses IR/NEC framing.
- The focused ASan/UBSan run covers this module and test only; it is not a full
  sanitizer suite. The on-TV run is CPU-only.
- Remaining synchronous modules (3): `plat`, `power`, `ui_omarchy`.
- Cold startup, scanout, real audio/CEC/tuner/PVR/VIN hardware, real VIN
  switching, the vendor `UserPreferences`/PQ object and broad screen/
  interaction parity remain unaccepted.
