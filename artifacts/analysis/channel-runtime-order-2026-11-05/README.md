# Channel runtime-order evidence

Interpretation:
[CHANNEL-RUNTIME-ORDER](../../reports/CHANNEL-RUNTIME-ORDER-2026-11-05.md).

- `check.txt`: full host suite with the active-list parser, socket and UI tests.
- `full-host-summary.txt`: host **423 ok**, `verify.cjs` **45 groups**.
- `arm-build.txt`: ABI 3 control, `mod_channels`, `mod_ui_vendor` and parser test.
- `arm-run.txt`: parser test under qemu-arm-static.
- `identities.txt`: host/ARM module and test hashes.

`current-fb1-no-signal_0.png` is the read-only fb1 capture taken during this
turn; it showed the no-signal standby screen, so no stock apps-grid oracle
exists yet. No input was sent and no display state was changed.
