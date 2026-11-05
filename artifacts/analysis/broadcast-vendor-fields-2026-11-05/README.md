# Broadcast vendor-field/EIT-cache evidence

Interpretation:
[BROADCAST-VENDOR-FIELDS](../../reports/BROADCAST-VENDOR-FIELDS-2026-11-05.md).

- `check.txt`: full host suite including the extended fixture/IPC tests and the
  new EIT schedule-cache test.
- `full-host-summary.txt`: host **415 ok**, `verify.cjs` **45 groups**.
- `arm-build.txt`: ABI 3 control binary plus ARM EIT tests.
- `arm-run.txt`: both EIT tests under qemu-arm-static.
- `identities.txt`: host binary/module/test and ARM test hashes.

No TV operation or vendor execution is involved.
