# Synchronous mixer dispatch and bounded wire codec — 2026-11-01

Later follow-ups: MIXER-MODULE supplies the nonblocking parent service/CLI.
SPEAKER-GAIN connects the direct setter to its raw cache and documents why the
getter's100-to0x8000 mapping prevents exact rollback. Its added lower setter
has injected-I/O coverage only; the production broker/service remain query-only.

Follow-up AUDIO-BROKER-2026-11-01 provides a separate-PID query executable and
validated local IPC, with a real spawned GetOPUGain exchange on TV. Host module/
event-loop integration remains open; worker threads alone cannot remove the
prelude's same-process PID binding. No gain/table/setter acceptance is claimed.

Status: dispatcher lifetime resolved; codec and owned blocking session now
implemented. Refactored read-only query passed on TV. Host worker/broker
integration, real gain/table writes, full audio init and audible acceptance
remain open. No gain/table/mute/reset write was executed.

## Completion and lifetime

The NDAL initializers use AF_NETLINK/SOCK_RAW/protocol2 and bind the process id.
PreInit _ker_nerve_nl_cb_p (4478c) matches installed command-name prefixes and
calls the matching callback directly, ignoring its return value. It neither
inspects ACK flags nor provides an operation-success acknowledgement.

Target kernel netlink_unicast (805ce308) calls the registered kernel callback
directly at805ce480 before consuming the skb and returning its original byte
count. The optional tap branch at805ce4c4 returns to the same callback. There is
no deferred worker on this path. Combined with Mod_SDAL_SetVolTbl's
copy_from_user (AUDIO-GAIN-TRANSPORT), the table pointer must remain valid through
the **whole send syscall**; the callback has finished and kernel storage owns
the table copy on return. Retain owner/buffer through errors too. Do not replay
EINTR blindly: an operation may have run. Blocking execution belongs outside
the event loop; SOCK_NONBLOCK does not bound kernel-handler execution time.

A full send count proves completed dispatch, not prefix matching, operation/DSP
success or audible output. Standard ACK flags cannot recover the discarded
callback result. AUDIO-GAIN-TRANSPORT's timing question is now resolved to this
scope; its status-loss findings still apply.

## Socket ownership

tdp_shell_beacon_send (33814) checks find_vpid(header pid) and unicasts to that
id. The bound socket/header id must match a real process id for replies. A
process whose HAL already owns that protocol2/pid binding cannot simply bind
another socket. Do not steal its socket or consume its replies. Native
integration needs one coordinated owner or a separate owning process. The new
diagnostic uses a fresh process and only its own socket.

## Implemented codec and probe

common/audio_netlink.c / audio_netlink.h encode the vendor8208-byte request:
16-byte header, type0/REQUEST/sequence0/own pid, command prefix immediately
followed by56 binary parameter bytes and a zero-filled tail. Explicit little-
endian operations avoid unaligned casts. Public builders support:

- GetOPUGain, normal devices0..5.
- VOLCfg, table indices0..9, nonnull/nonwrapping32-bit target user address and
  exactly101 entries. Encoding does not validate actual memory accessibility.
- SetOPUVolCfg, devices0..5 and table indices0..9.

Encoding is pure data; rejected inputs leave output unchanged. The decoder
requires a complete kernel GetOPUGain reply: header/prefix,56-byte payload,
trailing NUL and83-byte message (84-byte aligned delivery also accepted).
Alignment padding is ignored. Caller must separately verify actual kernel peer
port0 and recvmsg truncation flags. No lower-operation status exists in the reply.

New tools/audio/audio-netlink-probe.c only sends **GetOPUGain device0**. It
checks full send, polls up to2s after send, verifies kernel source/complete reply,
never replays an interrupted request and closes its socket. It links/initializes
no HAL, needs no ALSA configuration and offers no write option. Setter builders
are not executed by this probe or integrated into a production module yet.

Three unit groups pass strict host C11 -Wall -Wextra -Werror -pedantic,
ASan/UBSan and ARM/qemu: golden frames, packed fields, zero tails/canaries,
invalid capacities/ids/pointers/count/address wrap, complete/aligned replies and
atomic malformed-input rejection. make check-audio-netlink is in make check.

## Initial codec-probe TV confirmation (historical binary)

ARM probe28,376 bytes, MD5 **cea9dfda92951d1de0efe87d0d8bfdf9**. Upload and
size/hash verification were separate successful connections. Result, exit0:

    reply device=0 left=32768 right=32768; operation status is not present

This verifies the actual request/reply exchange and decoder. _cmd_get_opu_gain_p
(31644) calls Mod_Cell_GetOPUGain and normalizes returned100 to8000, without
including its lower result. The previous ALSA request-cache0 is a different layer
sampled at another time; the two getter APIs are not interchangeable. Neither
query result independently certifies DSP or audible behavior.

Probe and /tmp/oa-audio-netlink-20261101 removed; no helper running. Same boot
ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 and stock PID1737. Ethernet, SSH/telnet,
hosts/Samba blocks and hook/keeper hashes verified unchanged. No persistent
deployment or recovery change. Production core/curve DSO still
54a626f4c3a00b2e90490bc2626fe055 / a644c66e92bd6d53ef1032a990f8c4cc;
the new codec is not linked into those artifacts.

Avoided detours: GermBeacon loads/reloads command text; it is not ordinary
receive dispatch. _cmd_capability_p **sets bypass capability**, despite its
ambiguous name; it was not executed. Never send guessed commands as a delivery
test. Source identities remain those in AUDIO-GAIN-TRANSPORT; kernel evidence is
ker0_sym.elf reconstructed from this target's NAND, not a sibling kernel.

Evidence: ../analysis/cec-ownership-2026-11-01/audio-netlink-* and
live-audio-netlink-*. The follow-up below supersedes the initial probe's private
socket implementation/binary identity, retaining the firmware contracts above.

## Owned blocking session implementation

common/audio_client.c / audio_client.h implement a reusable session with
injectable I/O. Real I/O is enabled only for target little-endian ARM32. Open
creates raw/nonblocking/close-on-exec protocol2 socket and binds the current pid,
without HAL/hardware initialization. Bind failure unwinds only the new fd and
preserves primary errno, including competing-owner EADDRINUSE. Descriptor0 is
valid; reopening a live session rejects. Inherited child requests reject EPERM
before sending; child close releases its own reference, leaving parent usable.

Table and selection methods require a full send count. Zero means dispatched,
not applied. Invalid input never sends; short send fails, no error/EINTR replay.
Caller retains the target table address/buffer through the entire blocking call.
These methods have injected-I/O coverage only: no real table/selection send.

Gain query drains at most16 stale datagrams from its own socket before request;
excess fails without sending. It validates wait, actual kernel source,
truncation, reply shape and expected device. Error leaves output unchanged.
Reply timeout does not bound the synchronous kernel send. Serialize session use
outside the event loop; this library does not yet manage a host worker/broker.

Close is idempotent and consumes/resets fd even on Linux close error. Never retry
the old integer. This differs from CEC's fallible hardware-close ioctl and retained
ownership contract: the audio socket has no separate hardware teardown ioctl.

Five tests/audio_client.c groups pass strict host, ASan/UBSan and ARM/qemu:
fd0/flags/binding/stale drain; failure unwind/primary errno/final-close/reopen;
table/selection frames/no replay; timeout/source/truncation/shape/atomic recovery;
actual fork inheritance protection and continued parent use. check-audio-client
is included in make check; full host suite passes.

The refactored audio-netlink-probe delegates to this session. Current tested ARM
probe: **44,556 bytes**, MD5 **bfef5a6123cbece7d17dfee92440f5e3**. Separate upload
and hash/size verification preceded real-TV execution: device0/left32768/
right32768, exit0. No table/gain/mute/reset write. Probe and staging
/tmp/oa-audio-client-20261101 removed; same stock boot/PID1737, Ethernet,
SSH/telnet, hosts/Samba and hook/keeper hashes verified at baseline. No installed,
core/module or recovery artifact was rebuilt/deployed for this session change.

Follow-up evidence: audio-client-* and live-audio-client-* in this analysis
directory. Next integrate coordinated host worker/broker ownership and real
audio execution/acceptance, keeping dispatch/request/cache/hardware distinct.
