# App-manager idtv entry wrapper — 2026-11-07

Status: offline wrapper and host-only tests. Not installed; no boot hook or
persistent replacement was changed.

## What it is

`openaurora/tools/boot/run-aurora.sh` is the intended replacement for the
app-manager's idtv program on this unit. The observed appmgr argv is:

```text
/vendor/com.nvt.appmgr/com.nvt.appmgr --applist=/vendor/data/nvt.app.debug.ini
```

and `nvt.app.debug.ini` maps idtv to `/mtd_apexe/board/run_aurora.sh`
(the retail profile maps it to `/mtd_apexe/board/com.nvt.idtv/aurora.elf`).
The wrapper is bundled as `run-aurora.sh` but is not installed by the build,
`takeover-full.sh`, or any service file.

## Behavior

1. Resolve its own directory and accept `OA_STOCK` (default
   `/vendor/com.nvt.idtv/aurora.elf`) and `OA_APPLOG` (default
   `/tmp/oa-appmgr.log`).
2. `OA_DRYRUN=1` prints the supervisor, child and fallback command without
   executing anything.
3. Verify `openaurora`, `oa-supervise`, `openaurora.conf` and `modules/`.
   Any missing piece execs the vendor fallback.
4. If `manifest.sha256` exists, verify it with `sha256sum -c`; a mismatch execs
   the vendor fallback before starting anything.
5. Clear stale heartbeat/ready/socket files.
6. `exec oa-supervise --heartbeat ... --ready ... --wdt /buffer/wdtctl
   --fallback <stock> -- <bundle>/openaurora --modules ... --config ...
   --sock /tmp/openaurora.sock`.

`oa-supervise` owns the watchdog disable/re-arm, child restart, suite reload
(exit 75) and fallback exec. The wrapper does not disable the watchdog itself,
does not create a boot service and does not alter `/conf` or `/mtd_apexe`.

## Test evidence

`tests/appmgr_entry.sh` runs only shell stubs in a temporary directory:

```text
ok appmgr entry: dry run prints supervisor/child/fallback without execution
ok appmgr entry: incomplete bundle falls back to stock
ok appmgr entry: complete bundle hands off to oa-supervise
ok appmgr entry: manifest failure falls back to stock
all appmgr entry tests passed
```

The complete-bundle case asserts the exact argv elements: `--heartbeat`,
`--ready`, `--wdt /buffer/wdtctl`, `--fallback <stock>`, `--`, bundle child,
`--modules`, config and `--sock`. The manifest case proves the supervisor is
not executed on a hash mismatch. `make check-appmgr-entry` is part of
`make check`.

## Integration limits

- This is not installed at `/mtd_apexe/board/run_aurora.sh` and not selected by
  appmgr. Persistent boot integration needs explicit owner authorization and a
  recovery plan; the stock USB recovery image remains untouched.
- The appmgr `idtv` state machine expects `VOLCTRL|NODFB|IPCKEY` properties and
  a compliant process lifetime. The wrapper preserves the process by `exec`,
  but real appmgr launch/suspend/resume has not been exercised.
- `manifest.sha256` must include `run-aurora.sh`; the bundle rule updated
  accordingly.
- No TV, watchdog, `/buffer`, fb, HAL or network operation was performed.

Report: APP-MANAGER-ENTRY-2026-11-07. Related: APP-MANAGER-CONFIG,
REPLACEMENT-SYSTEM.
