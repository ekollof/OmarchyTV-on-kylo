# Live CPU-only module acceptance evidence

Interpretation:
[LIVE-MODULE-ACCEPTANCE](../../reports/LIVE-MODULE-ACCEPTANCE-2026-11-05.md).

- `live-session.txt`: full socket transcript from the TV run.
- `identities.txt`: host hashes and the hashes verified on the TV before
  execution.
- `module-imports.txt`: staged binary/module NEEDED libraries and absence of
  device-path strings.
- `cleanup.txt`: exact-PID shutdown, stock PID before/after, socket/staging
  removal, and the read-only testtool state.

This is a CPU-only live run. It does not cover fb1/display/HAL, testtool TCP
1986, vendor execution or stock takeover.
