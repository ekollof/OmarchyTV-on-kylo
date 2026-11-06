# Skin hot-switch from inside any skin — 2026-11-06

## Requirement

Any active skin must be able to load and switch to another skin (for example
OmarchyTV → vendor → diagnostic) without the control CLI being the only path.
The switch must not unload the calling skin while its callback is still on the
stack.

## Mechanism

`api->control(priv, "skin <name>", reply, cap)` is already available to every
module. A skin key/action can call it. The control handler now distinguishes
two cases:

- `cur_module == 0` (CLI/control connection): `oa_skin_request()` runs
  immediately, as before.
- `cur_module != 0` (inside a skin callback, timer or post): the request is
  stored in `h->skin_pending` and the reply is `pending <name>`. The new
  `oa_skin_pump()` runs in `oa_host_run()` after post dispatch, the scheduler,
  the module-batch pump and timers, i.e. after `cur_module` has unwound to 0.
  It then calls `oa_skin_request()`, which:
  - removes the old skin (`REMOVE`/`CLEAN` with quiesce retries),
  - loads/starts `ui_<name>`,
  - waits for the engine's `ui.skin` status to report the new owner/name,
  - falls back to vendor on failure, as before.

An immediate request supersedes a queued one (`skin_pending` is cleared).
A second deferred request while a switch is already running is rejected with
`EBUSY`; the caller can retry after the switch completes.

## Demonstration keys

`ui_omarchy` and `ui_diag` bind project control key **240** (`emit-key 240`,
not a vendor IR code):

- omarchy: request `skin diag`
- diag: request `skin omarchy`
- vendor remains available via `skin vendor` from the CLI or a skin action;
  it needs the vendor `ui.dir` assets.

## Tests

`tests/skin_switch_ipc.cjs` starts `nav_display` + `ui_engine` + `ui_omarchy`
with the fake display, then:

1. asserts `skin` reports `active omarchy`,
2. emits key 240 (inside the omarchy key callback) and waits for
   `active diag`,
3. emits key 240 again (inside diag) and waits for `active omarchy`,
4. stops/unloads cleanly and exits 0.

```text
ok skin switch: omarchy -> diag -> omarchy from skin key callbacks
```

`check-skin-switch` is part of `make check`; the full suite passes. The test
also covers the deferred path: without the host queue the skin request would
return `EBUSY` because the calling skin is still on the stack.

## Limits

- Only one skin is active at a time; the old skin is unloaded. Reload state
  handoff applies only where the skin implements it.
- The fallback target is vendor; if vendor assets are missing the switch fails
  and the engine reports the error.
- Key 240 is an OpenAurora control key, not a vendor remote code. Skins may
  bind `skin <name>` to any key or Lua action.
- Hardware/display acceptance is unchanged and still pending.
