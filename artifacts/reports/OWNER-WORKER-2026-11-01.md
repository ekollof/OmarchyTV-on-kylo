# Owning CEC and audio workers — 2026-11-01

Review follow-up: WORK-REVIEW-2026-11-01 supersedes the 500ms idle wait below.
Quiesce is now nonblocking, pending reload can retry release, and the loop-thread
bind_session service rejects every argument until asynchronous completion is
implemented. Lower owner RPCs block their caller and are for non-loop use only.
The original injected-I/O and hardware-acceptance limits remain in force.

Blocking CEC and mixer calls now run on one worker thread each. The host loop
does not call the device. `on_settle` runs on the worker after the owner lock
is released and may `post()` back to the host.

## CEC

`oa_cec_authority_ensure` follows the VPL_CEC_Open step recovered at 0x616ce4.
The caller supplies the mirror read and the register write. A set bit 1 returns
without a write. A clear bit writes STBC register 0xf011 with the mirror low
byte plus bit 1. The helper returns 0 only when that write returns 1, matching
DRV_STBC_WriteRegister. It does not map physical 0xfc040200 and does not send
STBC IPC.

`oa_cec_owner` runs acquire, send, receive and release on its thread. Acquire
requires authority, device I/O and a non-empty path before it calls ensure, then
`oa_cec_device_open`. A NULL device table is EINVAL and does not read the
mirror. Shutdown attempts `oa_cec_device_close` outside the lock. A failed
hardware close keeps the fd and session. The thread waits until a later
`quiesce` advances the generation, then tries close again. That failure does
not call `on_settle`, so a host whose only wake would be that post waits in
poll. A successful close, or a worker that never opened, sets exited, then
calls `on_settle`. `join` is allowed only after exit.

`mod_cec` is an ABI 3 module. `start` creates the idle worker and publishes
`cec.owner` v1. `hardware_open_supported()` returns 0. The module does not map
the authority word, send STBC IPC, or open ntkcec. `quiesce` returns busy until
the worker has exited. `stop` joins and destroys the owner. The first cut had
no suspend or resume hook. That is superseded by the lifecycle follow-up below.
A failed hardware close still cannot be unloaded by spinning the loop; the host
waits for some other wake to retry quiesce.

## Audio

`oa_audio_owner` uses the same thread shape around `oa_audio_client`. Open
rejects a NULL I/O table with ENOTSUP before any socket call, including on ARM,
so the real HAL netlink socket cannot be bound by accident. Gain and close are
jobs. Shutdown close uses `oa_audio_client_close`, which drops the session
before the OS close, then the thread exits and calls `on_settle`. There is no
generation retry. The first cut left this owner out of `mod_audio`. The module
now creates it; see the lifecycle follow-up. It still does not select the real
HAL socket.

## Tests

`tests/cec_owner.c` covers the authority bit, the injected acquire/send path,
two failed-close generations that keep the fd, a later successful close, and
host shutdown of the idle module. `tests/audio_owner.c` covers the NULL-I/O
rejection, a GetOPUGain reply on the worker thread, and close during quiesce.
The gain fake returns EAGAIN until the request is sent, matching the client's
stale-reply drain.

All of these pass on the strict host, under ASan/UBSan (`build-owner-san`), and
under `qemu-arm-static -L artifacts/workbench/rootfs/root` using
`build-owner-arm`. The full host `make check` also passes. The ARM `make
check-*` recipe still executes the binary directly and is not that qemu result.

`build-tv-v3/openaurora` was not relinked. Its MD5 remains
fc9eb71f924dd850c7ba4f1c9b8a02cd. `mod_cec.so` is not in that stage set and was
not loaded on the TV. No ntkcec open, STBC write, netlink bind, amplifier
access, or persistent deploy was performed.

## Lifecycle follow-up

`mod_cec` suspend and resume join and recreate the idle worker. The published
`cec.owner` service stays in place across suspend. Stop still joins, then
unpublishes. A failed hardware close makes suspend and stop return busy and
leaves the owner in place.

`mod_audio` creates the same audio owner after it publishes `audio` v1.
`bind_session(NULL)` returns ENOTSUP before any mixer socket. The host build is
`OA_NO_VENDOR`, so `audio.enable` cannot turn the HAL on there. Suspend of that
disabled module joins the idle worker. A separate test DSO, built with
`-UOA_NO_VENDOR` and `tests/audio_hal_stub.c`, keeps `audio.enable=1` started
and makes suspend return `suspend failed`. That stub is not the TV HAL.

Idle `oa_cec_owner_quiesce` and `oa_audio_owner_quiesce` wait on the worker
condition for at most 500ms, because the worker sets `exited` before
`on_settle` and one CLI reload does not retry. An open CEC session returns
busy without waiting. Each CEC call still advances the close generation. An
open audio fd is also not waited on; the worker closes it and a later quiesce
observes the exit. Shutdown of a failed CEC close still returns to `poll`
instead of spinning.

`tests/run.sh` loads `plat` before `panel`, then reloads cec and audio and
suspends and resumes them on the running host. `tests/cec_owner.c` covers the
held-service unload refusal. `tests/audio_session.c` covers the enabled-HAL
suspend failure. The same suite passed twice on the host, twice under
ASan/UBSan (`build-owner-san`), and twice under qemu-arm with the TV rootfs
`/lib` mounted in a namespace (`build-owner-arm`). `make check` on an ARM
binary without that namespace is not the qemu result: `check.c` execs
`/lib/ld-linux-armhf.so.3` directly. No `build-tv-v3` relink and no TV deploy.

## System Audio and ARC route

`mod_cec` also publishes `cec.audio` v1 from `common/cec_policy.c`. The service
handles injected frames only. It does not open ntkcec, map `0xfc040200`, write
STBC, bind netlink, or call `Hal_Audio_Ctrl`. `hardware_open_supported()` stays 0.

CEC 1.3a System Audio Mode turns on only from Set System Audio Mode `0x72` or
System Audio Mode Status `0x7E`. A System Audio Mode Request `0x70` is built for
logical address 5 and does not flip the mode. While the mode is on, the route
marks the TV speakers muted and local volume is not permitted. `mod_audio`
checks that before volume opcode `0x33` and mute, returns `-1`, and leaves its
cache unchanged. Report Audio Status `0x7A` is stored as external UI state,
including muted volume 40 (`0xA8`) and unknown 127. It is not a speaker write.
Volume Up `0x41`, Volume Down `0x42` and Mute `0x43` go out as User Control
Pressed `0x44` followed by Released `0x45`. Give Audio Status `0x71` and Give
System Audio Mode Status `0x7D` directed at the TV are Feature Abort Refused
(reason 4). Broadcasts of those are ignored. Directed malformed audio frames
are Feature Abort Invalid operand (reason 3).

CEC 1.4 ARC keeps the TV as the transmitter. The TV emits Request ARC
Initiation `0xC3` and Request ARC Termination `0xC4`. It answers the amplifier's
Initiate ARC `0xC0` with Report ARC Initiated `0xC1`, and Terminate ARC `0xC5`
with Report ARC Terminated `0xC2`. It does not send `0xC0` or `0xC5`. Adjacency
and the ability to transmit are injected and default to off, so a direction-valid
`0xC0` Feature Aborts Refused and leaves `arc` clear. Adjacent but unable to
transmit Feature Aborts Cannot provide source (reason 2). Both flags set stores
`arc` only after the `0xC1` reply is stored. ARC does not mute the speakers or
redirect volume. A wrong initiator or a TV-role opcode directed at the TV is
Refused. Broadcast ARC produces no abort.

Reload saves eight bytes: mode, arc, external status, link flags and the pressed
UI key. Restore rejects any other length, a pressed key while the mode is off,
and an unknown external volume other than 255. `tests/cec_audio_policy.c` loads
real `mod_plat.so`, the HAL-stub `mod_audio.so` and real `mod_cec.so`, drives
the published services and `reload cec`, and checks `status cec` still lists
`cec.owner:v1` and `cec.audio:v1` as started. That test passed `make check`,
once under ASan/UBSan (`build-owner-san`), and once under qemu-arm with the TV
rootfs `/lib` namespace (`build-owner-arm`). This is not electrical ARC, a live
CEC exchange, or amplifier acceptance. `build-tv-v3` was not relinked.
