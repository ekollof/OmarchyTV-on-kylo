# Control client timeout / fire-and-forget — 2026-11-05

## Problem

A `oa-client` connection that sent `reload ir` while the module had both BLAST
(`/dev/video28`) and the MCU UART (`/dev/ttyS2`) open waited for the host to
finish the stop. The TV log shows the host loop blocked for about **64 s**
between the last key paint and `stopped ir` / `unloaded ir`:

```
[852312] painted portal ...
[916756] stopped ir
[916756] unloaded ir
[916762] ir: reloaded (started)
```

The client process was killed by the operator before the reply. The exact
driver call in the dual-open stop was not isolated; the earlier BLAST-only
`reload ir` completed promptly.

## Client changes

`tools/probe/oa-client.c` now:

- accepts `--timeout SECONDS` (default 5, max 3600);
- sets `SO_RCVTIMEO`/`SO_SNDTIMEO` and polls for writable sockets;
- prints `oa-client: timeout after Ns waiting for a reply` and exits 1 instead
  of waiting forever;
- accepts `--no-wait`, which queues the commands, closes the socket and exits 0
  without reading a reply. Use this for a potentially slow reload, then poll
  `status <name>` separately.

`tests/oa_client.cjs` starts a Unix server that accepts and never replies:

```
ok oa-client: bounded timeout and no-wait reload
```

The measured cases were 1.02 s for `--timeout 1` and 0.00 s for `--no-wait`.

## IR default

`mod_ir` now treats BLAST as the primary path. With no explicit `ir.device`, it
does **not** open `/dev/ttyS2` when BLAST started successfully. The UART still
opens when:

- `ir.device` is set explicitly, or
- BLAST is disabled/unavailable (the old fallback behavior).

This removes the dual-open stall from the normal physical-remote configuration.
The BLAST-only and UART-only host tests still pass.

## Host async cleanup gate

`mod_ir` now follows the `display_fb` model. `m_quiesce` cancels its timer and
watch, transfers the UART fd, BLAST v4l2 fd, `libNDAL_EX` handle and close
function to a worker, and returns `-EAGAIN` while that worker runs. The host
keeps the module in `cleanup_pending`; the first `reload`/`stop` fails fast and
a later retry joins the worker and completes the unload.

`tests/ir_quiesce.cjs` uses a real pty as `ir.device` and a diagnostic
`ir.cleanup_delay_ms=400` to hold the worker open:

```
ok ir quiesce: reload fails fast, stays responsive, retry completes
```

The test asserts that the first reload returns `failed` in under 1.5 s, that a
`status ir` command still answers in under 1 s while the worker runs, and that a
600 ms retry plus the subsequent stop/unload complete. `check-ir-ipc`,
`check-full-stack-ipc` and the ARM `mod_ir.so` (libpthread, max GLIBC 2.4)
still pass.

## Host batch queue, 2026-11-06

The client no longer waits, but the host still executed `load`/`start` inline:
`oa_control_command` called `oa_module_load`/`oa_module_start` and only then
returned, so `load a; load b; load c` on one connection paid every init in one
turn. The module scheduler is not the fix: its published contract says a step
runs on the loop thread and **must not call module lifecycle**
(`include/openaurora/services.h`), and it runs under an owning module. Lifecycle
stays host-owned.

Additive commands now exist and leave `load`/`start` unchanged:

```
load-batch <names...>    queue dlopen+init, one entry per loop turn
start-batch <names...>   queue load+start, one entry per loop turn
```

- The reply is `queued N` (plus `dropped M` when the 64-entry FIFO or a
  name of 128+ bytes is refused); it never waits for a module.
- `oa_control_pump()` runs at the top of `oa_host_run()`, after the scheduler,
  and performs exactly one entry per call. Missing modules are logged and do
  not stall later entries; re-queuing a loaded/started module is a no-op.
- `oa_control_pending()` makes `poll_once()` use a zero timeout while awake
  work remains, so the queue drains without new socket input. It returns false
  during `sleep_cycle`/`shutting_down`, so queued work pauses and resumes
  instead of spinning. Entries are cleared on control start/stop.

Limit: the queue bounds a burst, not one call. A single module whose `init` or
`start` blocks still blocks that loop turn; making an individual lifecycle call
asynchronous needs its own quiesce-style gate (as `mod_ir` has for stop).

Evidence:

```
ok batch_load: one module per pump and nothing before the pump
ok batch_load: start-batch starts one module per pump
ok batch_load: missing entry does not stall the rest
ok batch_load: bounded queue reports drops and drains fully
ok batch_load: oversized name is rejected
ok batch_load: missing names give usage, not a partial run
ok batch load: socket queue drains one module per loop turn
```

`tests/batch_load.c` drives three real fixture DSOs and asserts module state
after each pump; `tests/batch_ipc.cjs` pipelines `load-batch ...` and
`status ui_anim` in one write, gets `queued 3` + `no such module: ui_anim`,
then observes all three load/start without sending another command. Live CLI:
`oa-client --no-wait ... 'load-batch ticker ui_clock ui_anim'` returned in
3 ms and all three reached `loaded`. Focused ASan/UBSan, the full host suite
and the ARM `build-tv-v3/openaurora` build pass.

## Limits

- Only `mod_ir` has the nonblocking cleanup gate. A slow stop in another module
  can still occupy the event loop; the generic lifecycle remains synchronous,
  and the batch queue does not change that for one call.
- `--no-wait` returns before the host completes; callers must poll
  `status <name>` / retry the stop or reload.
- The exact 64 s vendor-driver stall was not reproduced in isolation; the gate
  is validated with a controlled 400 ms worker delay.
