# Read-only testtool/EASI probe evidence

Interpretation: [EASI-PROBE](../../reports/EASI-PROBE-2026-11-05.md).

- `check.txt`: local fixture test for `ACTIVECHANNELLIST` XML parsing and
  non-read command rejection.
- `full-host-summary.txt`: full host **411 ok**, `verify.cjs` **45 groups**.
- `arm-build.txt`: fresh ARM build of the probe.
- `identities.txt`: host and ARM probe hashes.

The TV testtool socket is disabled (`is_testtool_socket_enabled=0`, TCP 1986
refused). The probe is ready to run when the server is enabled; no vendor
process was paused or restarted.
