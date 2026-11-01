# Reloadable mixer-query service and control commands

mod_audio_mixer is now an ABI3 module publishing audio.mixer v1. It uses the
separate oa-audio-broker executable, with no HAL/platform dependency or setters.
Explicit audio_mixer.broker configuration selects the matching helper. Core
commands mixer-query, mixer-result and mixer-cancel submit/collect/discard values
through the real control socket or stdin.

The module uses one bounded pending/result slot, nonblocking send/receive,
32-byte codec validation and matching ticket/device. Tickets combine the module
owner token and a bounded sequence, retain their sequence across stop/start, and cannot
cross reload. Invalid numeric CLI input rejects rather than wrapping/truncating.
Ready values are collected once; pending take preserves output. Cancel drops
ready values or drains a pending reply before freeing the slot. Consumers hold
the service and cancel abandoned tickets before releasing it. No caller pointer
or completion callback is borrowed, and no writable hardware API is exposed.

Watch/reap timer registration happens in module start under its provider token.
IPC close consumes the fd without replay. Quiesce rejects new requests, drops
pending values, closes IPC and returns busy until waitpid(WNOHANG) reaps the
child. A50ms timer drives completion/retry while the host continues dispatching.
The module never blocks on child wait or native mixer I/O. Buffered final replies
are read before reporting an unexpectedly exited peer. Broken/mismatched/oversize
responses terminate the channel and return a matching-ticket error result.

Suspend closes/reaps the child while preserving service lookup holds. Resume
starts another child without unpublishing the service or resetting the sequence.
Failed resume explicitly closes partial sockets/watches, because the core does
not call stop after resume failure. Start reserves watch/timer capacity before
spawning; failures unwind without an orphan child or stale publication.

Paired-hook verification uses explicit retry. A busy suspend may already have
begun quiescing; the sleep engine does not yet restore a partially quiesced
failed hook when preparation aborts. Async prepare/rollback coordination remains
in TODO. This is not native standby or failed-prepare rollback acceptance.

## Firmware correction: spawn success is not exec success

An initial fixture wrongly assumed the current host libc's immediate ENOENT
behavior. Minimal spawn-absent.c returns rc2/no child on the host but rc0 and a
child exiting127 against target libc in ARM/qemu. This is firmware behavior,
not an emulator defect. Target libc MD56ffe6403c8a3794a4fea26d676b45240 matches
the TV exactly. Its posix_spawn at0x46fb8164 calls internal
0x46fb81f0; __fork at0x46fb822c precedes the parent's PID store/return0 at
0x46fb8240/8242. The child path calls execve and failure reaches _exit127
at0x46fb83d2/83da. New raw disassembly/minimal reproduction is preserved.

An X_OK precheck catches an obviously missing helper before resource allocation.
It is not an exec/readiness guarantee: the module also handles delayed EOF and
reaps a child whose existing script has a missing interpreter. Tests accept the
valid immediate or asynchronous libc failure path and exercise both environments.
No core/helper/HAL health is inferred merely from posix_spawn returning0.

## Verification and target state

Five real-DSO groups pass strict host, ASan/UBSan, ARM/qemu and real TV:

- Bounded requests, cancellation/drain, owned timer/watch, held-provider unload
  refusal, hold retention across suspend, failed-resume fd counts and reload tickets.
- Wrong ticket, oversize, EOF, explicit error, buffered final reply and shutdown
  during a nonresponding request, with no child left unreaped.
- Missing program, full timer/watch tables and cleanup without stale services.
- Missing exec interpreter with immediate/asynchronous failure and reaping.
- Real control socket, two query/collect/reload rounds, invalid CLI input,
  stale-ticket rejection and orderly host shutdown with no orphan children.

ARM/qemu runs the ARM core/DSO/test against target rootfs with a native host fake
peer executable; IPC codec is endian-explicit. It does not emulate TV devices.
On TV, the full fake-peer suite also executes on ARM. A separate --native socket
run uses the real broker and device0 only: both rounds returned error0 and
left/right32768, with provider reload and old-ticket rejection. A standalone
core invocation with explicit temporary config also passed start/status/quit.
These values are query data, not an operation/DSP/audible ACK or full audio parity.

Full strict host make check passes. Focused module/control sanitizer checks
pass. Native staged core+two selected modules passed --check before execution.

| artifact in runtime stage | MD5 |
|---|---|
| core (452616bytes) |08f549d4026cfb626c1ffc93b2e02fc6|
| audio_mixer DSO (43312bytes) |f78e63e378ae31e48a7cf37a1640b92d|
| native test (475792bytes) |7e26e0e32913e4e961269a6efd74af3e|
| broker |2e2856c667df2a40d2cbb38589a6753d|

The complete current ABI3 core+16 modules/helper/supervisor was then rebuilt in
build-tv-v3 and hash-verified on TV. Core+all16 modules passed --check, no module
startup in that preflight. stage.md5 identifies the matching set. The previous
ABI3 corefc9.../13-module stage, supervisor and two fixtures are archived in
artifacts/archive/build-tv-v3-before-mixer-20261101.tar. Every old stage.md5
entry was verified after archiving; SHA256 is recorded beside the tar. Installed
ABI1, historical ABI2 and recovery artifacts were not overwritten.

Foreground uploads and separate size/hash verification preceded execution.
Both /tmp/oa-mixer-module-20261101 and /tmp/oa-mixer-matching-20261101 were removed.
No helper/core/fake peer was left running. Stock boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197 and Aurora1785 remained unchanged, as did
installed core/hook/hosts hashes, SSH/telnet listeners, safety markers and Samba
REJECT. No stock UI restart, drawing, source/power change or hardware write.

Raw evidence: ../analysis/cec-ownership-2026-11-01/mixer-module-*,
live-mixer-module-*, live-mixer-matching-* and spawn-*. The original failed test
logs are retained as diagnosis history; corrected runs pass. Expected cleanup
pending logs during explicit retry are not crashes. mixer-module-tested-tv.tar
and its md5 preserve the exact runtime set; mixer-matching-tested-tv.md5 records
the complete16-module preflight set.

Next recover/confirm the native speaker-gain setter's operands/units, then add
AQ-table selection and UI gain through the isolated broker. Decoder opcode33,
speaker gain and amp register03 remain distinct. No gain/table/profile/mute
write is accepted yet. Full amp/routing/standby/native ownership and UI parity
remain open; privacy/telemetry exclusions are unchanged.
