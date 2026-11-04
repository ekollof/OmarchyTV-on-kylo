# Display cleanup through the real host/loader — 2026-11-04

The production display_fb code now has a maintained actual-host/DSO lifecycle
test. It exposed and fixed a host shutdown wait defect that the prior direct
module tests did not cover. All work was offline; no TV/fb1 access occurred.

## Observed shutdown defect

The first test reached its30-second alarm. A diagnostic rerun under host GDB
stopped immediately before poll in shutdown_modules: shutting_down=1,
timeout=-1, entered=released=beats=1, and the worker had already exited. After
the heartbeat timer released the blocked driver fixture and removed itself,
the host could enter an infinite poll without another event to retry join.
No fault in the driver, ARM guest or TV was inferred from that timeout.

Evidence: before-timeout.txt and before-gdb.txt in
`artifacts/analysis/display-dso-2026-11-04/`. Before-fix host executable SHA256:
`f4bec821d0b212695eb3c4bc5cf66bb7914d3c7872608cdb8b688897ee766011`;
host source was f0ea5da. The GDB backtrace identifies poll_once ->
shutdown_modules -> oa_host_run -> the final test case.

`core/host.c` now limits a shutdown poll wait to20ms, retaining any earlier
timer/heartbeat deadline and normal fd readiness handling. Ordinary operation
is unchanged. This scheduling interval is explicit project policy, not a
firmware constant. It provides the promised gate retries even without a post,
watch, timer or later heartbeat; it never forces an unsuccessful gate to release.
Permanent cleanup failure can still keep shutdown pending indefinitely.

## Fixture boundary and exercised behavior

`check-display-lifecycle` loads an actual display_fb DSO through the production
host and drives the production control-command parser. The DSO is built from
the production sources with test-only linker wrappers and a constructor/
destructor observer. The executable exports the fake device operations; no
production injection API or renderer is changed. The only accepted device path
is `/dev/oa-fixture`, never fb1. These DSOs live under display-fixture, not the
production modules directory, and are not deployment artifacts.

Five groups cover:

- Actual service holds refuse normal reload before quiescence begins, leaving
  rendering usable. A real ticker DSO supplies the consumer identity/hold.
- A blocked cleanup worker retains the same module ID, handle, context,
  service and fd. Status remains queryable; start and forced reload/unload
  refuse. After release/join, destructor observation proves finalization, and
  actual reload republishes from a new owner ID with restored backing/pan.
- The firmware-derived suspended-pan EFAULT aborts synchronous suspend. Retry
  succeeds once the fixture permits pan; reload while suspended does no device
  open, and resume performs deferred activation.
- A collision on display.alpha causes partial start to fail after layer setup.
  The host retains that loaded/cleanup-pending module through a blocked restore,
  including forced unload, then unloads after cleanup/join without leaked
  display registration or fake fd.
- Actual host shutdown dispatches a heartbeat timer while the driver is blocked.
  The timer releases the fixture and deletes itself; no later notification is
  supplied. Shutdown must retry join and finalize all modules with exact
  constructor/destructor and open/close balance. This reproduces the old hang.

This exercises control dispatch in process, not a new external Unix-socket
client test. Wrapped driver responses are controlled test evidence; no live
driver restoration, visual parity or exclusive layer ownership is established.
Firmware teardown/suspend comparison remains in DISPLAY-CLEANUP: the recovery
layer is project reload policy, distinct from vendor TDGA/PMI lifecycle.

## Verification and continuation

Strict host, leak-enabled ASan/UBSan and a fresh ARM/qemu test pass all five
groups. ARM build: TARGET=tv BUILD=build-tv-v3/display-dso, maintained compiler,
targets tests/display_lifecycle, display-fixture/mod_display_fb.so and
modules/mod_ticker.so. Run qemu-arm-static with the compiler's libc sysroot
and absolute fixture/ticker paths. Commands/logs and final executable/DSO
SHA256s are preserved in the analysis directory. No new vendor reference body
was changed or claimed executed by this host scheduling fix.
Full strict host check passes387 ok, exit0 (full-host.txt). Final ARM test
SHA256: `86ca02b5b2ac3dee8cc774afac2e7a70db4cc0ee4d4cbba2ff3d7962a97c0487`.

The controlled actual-host/DSO gap from DISPLAY-CLEANUP is closed. Exclusive
layer authority, live-driver equality, cold/native startup, complete kernel
power/standby/resume and fallback remain open. No persistent deployment or
hardware restoration gate is closed. Keep active fb1 untouched.
