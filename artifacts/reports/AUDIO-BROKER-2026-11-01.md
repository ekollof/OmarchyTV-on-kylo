# Separate-PID mixer query broker

Follow-up MIXER-MODULE-2026-11-01 adds the nonblocking parent module/service and
control commands, with real TV query/reload/reaping and complete16-module loader
preflight. The child-side details below remain valid; the old “next parent”
paragraph is historical. Writable/full audio acceptance still remains open.

The native parent cannot safely reserve another same-PID protocol2 socket after
the HAL's prelude bound that port. Fresh read-only Ghidra decompilation and ARM
instructions connect Hal_Audio_Mixer_Ctrl_Init4e7ad940 to Hal_IfCmd_Init through
its PLT call4e7ad97a→4e7ab924. The earlier initializer instructions at4e7ca16c
show socket(AF_NETLINK, SOCK_RAW,2), getpid and bind. mod_plat calls the mixer
initializer in its mandatory prelude. This is an evidenced process-ownership
constraint, not an authorization to initialize the HAL under the stock UI.
No new cold-init probe was performed.

oa-audio-broker is a separate executable with an AF_UNIX/SOCK_SEQPACKET connection
on stdin. It links libc and the maintained client/codec, not NDAL. Its entry point
closes unrelated inherited descriptors by enumerating /proc/self/fd (never
reading descriptor targets). Only after validating a request does it open its
PID-bound native netlink session. Subsequent queries reuse the same session.
IPC EOF closes that session and lets the child exit. No gain/table/mute/reset
setter, HAL init, amplifier access or CEC activity is implemented.

The local OAB1 protocol is our IPC, not vendor netlink: exactly32 little-endian
bytes, magic/op, nonzero64-bit ticket, device0..5, bounded query timeout≤2000ms
and zero reserved fields. Reply contains the echoed ticket, errno and gain
values; error replies zero values. Decode/encode rejection preserves output.
Oversized/truncated/invalid packets terminate before mixer open/send. Kernel
EINTR is returned as an error without replay. A gain reply is transport/query
data, not receiver/DSP/audible-operation acknowledgement.

Tests cover wire/endian/error rejection, one lazy reused injected session,
error propagation/no replay, malformed IPC before device access, wrong socket
type, and a real posix_spawn child with inherited-descriptor removal, IPC EOF
cleanup and waitpid reaping. Four groups pass strict host and ASan/UBSan. Full
host make check passes. Explicit ARM/qemu runs the injected codec/serve groups;
it does not pretend the workstation kernel has the TV mixer. The ARM make
recipe omits the host-only ENOTSUP spawn expectation. Actual TV uses --native.

The initial ARM test executable referenced fcntl@GLIBC2.28 and failed under
target-rootfs qemu before any TV upload. That fixture's integer-only F_DUPFD now
uses SYS_fcntl; no fcntl64 lock layout is involved. The corrected parent needs
at most GLIBC2.15 and helper2.4. This is not evidence of a missing TV driver.
Initial-loader log is retained separately.

After foreground upload and separate size/hash verification, the TV loader's
--list passed for both binaries. The native parent posix_spawn'ed helper PID19368,
which returned device0 left32768/right32768 through IPC, closed its session on
EOF and exited/reaped with status0. The high unrelated inherited fd was absent
from the helper. No backend descriptor was borrowed from stock Aurora.

| tested artifact | bytes | MD5 |
|---|---:|---|
| parent test |85640|bf227ea86b93b3a8c0c0055250fe0348|
| helper |62176|2e2856c667df2a40d2cbb38589a6753d|

Raw logs: ../analysis/cec-ownership-2026-11-01/audio-broker-* and
live-audio-broker-*. Exact tested parent/helper ELFs are preserved there.
audio-broker-prelude.txt is the new read-only Ghidra output, source NDAL3 MD5
c842bad804302ec822076c3d9d3ce31d; audio-netlink-socket.txt retains the initializer
instructions. The project was opened -readOnly -noanalysis, with no script error.

Temporary /tmp/oa-audio-broker-20261101 was removed. Boot
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, Aurora1785, installed core/hook/hosts hashes,
SSH22/telnet2323, safety markers and Samba REJECT stayed unchanged. No deployment,
restart, persistent configuration or recovery artifact changed.

This implements the blocking child endpoint, not a host module. Next add the
nonblocking parent adapter, request/result matching, owned fd watches, module
service holds and child WNOHANG reaping/quiescence, then control-plane access.
Do not call serve() or blocking waitpid in the host event loop. Writable audio
operations/full initialization/routing/standby/audible acceptance remain open.
