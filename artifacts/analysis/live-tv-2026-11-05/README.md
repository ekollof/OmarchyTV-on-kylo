# Live TV acceptance evidence

Interpretation: [LIVE-TV-ACCEPTANCE](../../reports/LIVE-TV-ACCEPTANCE-2026-11-05.md).

- `buffer-hashes.txt`, `staged-hashes.txt`: 38 staged files, SHA256 matched.
- `display-active-buffer.png`: fb1 while `display_fb` self-test was active
  (magenta band).
- `display-restored-buffer.png`: fb1 after stop/unload; blank, same as the
  pre-test frame.
- `display-buffer.log`: snapshot, self-test, cleanup retry and stop.
- `buffer-modules.txt`: PVR add/list/reload and Lua lifecycle over the socket.
- `ndal-tuner.txt`, `ndal.log`: explicit NDAL frontend scan result (EBUSY).
- `dvb-status.txt`: read-only frontend inventory (`frequency=0`, no lock).
- `eit-dvb-probe.txt`: direct Linux DVB section-filter probe (`sections=0`).
- `final-state-buffer.txt`: stock PIDs, absent staging, free `/buffer`.
- `host-check.txt`, `verify.txt`: 434 host ok / 45 verifier groups after the
  libm and direct-DVB fixes.

No firmware, boot hook or persistent state was changed. `/buffer` staging was
removed after the run.
