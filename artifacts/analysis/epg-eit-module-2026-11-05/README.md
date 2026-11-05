# EIT parser/EPG module evidence

Interpretation: [EPG-EIT-MODULE](../../reports/EPG-EIT-MODULE-2026-11-05.md).

- `check.txt`: full host suite with the EIT section parser, cache and real
  socket EPG module tests.
- `full-host-summary.txt`: host **417 ok**, `verify.cjs` **45 groups**.
- `arm-build.txt`: ABI 3 control, `mod_epg.so` and the parser test for ARM.
- `arm-run.txt`: parser test under qemu-arm-static.
- `identities.txt`: host and ARM module/test hashes.

No TV operation, tuner access or vendor execution is involved.
