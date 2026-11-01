# Audio gain transport, status and readable mixer state — 2026-11-01

Status: additional firmware contracts recovered and a read-only ALSA query passed
on the TV. This changes the next integration step: native gain/table application
must handle transport/result and lifetime explicitly. No gain/mute/table write
was executed or added to the production audio module here.
Follow-up AUDIO-NETLINK-2026-11-01 resolves dispatcher timing from target kernel
instructions and records a tested codec/native query. Native execution/operation
verification remains open; the status-loss findings below are unchanged.

## A scalar return is not necessarily an applied-operation result

The instruction-verified int prototypes from CEC-AUDIO-CONTRACT remain correct.
Do not revert them to Ghidra's void metadata. However, message-backed commands
such as Ctrl2e/3e/33 go through **Hal_Audio_Send** (NDAL3 4e7aec58):

- It formats a command string plus the56-byte parameter block in a shared buffer
  while holding its audio lock and calls _pl_mixer_cmd_send_p (4e7ad68c).
- That sender constructs a netlink message and calls sendmsg; failures are logged.
- Hal_Audio_Send returns0 for nonnull inputs regardless of the send result;
  ARM instructions explicitly restore its zero return register after the call.
  Thus Ctrl0 does not establish delivery, receiver success or DSP acceptance.

For example, the command table at4e7f3878 maps2e→VOLCfg and3e→SetOPUVolCfg.
The table's string pointers are the command names, not numeric ioctl ids.
Hal_Audio_Send alone is not a checked native transport implementation.

New analysed project: artifacts/workbench/ghidra_alsa, Alsa / ntkndpalsa.ko,
MD5 **8758a13d12799217ce702849df7d54da**, matching the live file. Existing NDAL3
and ntkdriver source identities remain c842bad804302ec822076c3d9d3ce31d and
fded7005651e37b8a1b5e8e89f73f4ff. New project manifest is
analysis/cec-ownership-2026-11-01/alsa-project-SHA256SUMS.

## Receiver and table lifetime

ntkndpalsa _beacon_cmd_cb_p (33670) dispatches the registered command handler.
_cmd_vol_cfg_p (32140) calls Mod_Customer_SetVolTbl and Mod_Cell_FlushVol and
computes a failure result. That result is not propagated by the user wrapper.
The target ntkdriver dispatch at1d6cd4 selects Mod_SDAL_SetVolTbl (1ddda0) via
the operations table at6aaf88. It uses copy_from_user to copy bounded halfword
entries into kernel-owned table storage; remaining configured entries get8000.
The kernel table does not retain that userspace pointer after the copy.

This establishes the receiver's copy behavior, **not** the complete timing/
acknowledgement contract of the parent netlink dispatcher. Do not pass a transient
stack table or release a reloadable owner until delivery/completion/lifetime has
been established. Native transport/authority/worker/cleanup remains work.

## Speaker control and the limit of readback

Hal_Audio_KItem (4e7ae9c8) searches256 initialized124-byte descriptors. It uses
the id at descriptor+120, not an array index. Live initialized table at4e7f8208
was read from the verified vendor process; record168 contains:

    id=42(hex) name="speaker out gain" card=0 type=2(integer)

Hal_Audio_EasySet uses the named ALSA control. Its synchronous ALSA wrapper checks
the library call result, but ntkndpalsa speaker_out_gain_put (2b338) calls its
helper and then returns0 unconditionally. In helper2945c case7, the value is
stored in configuration+ a0 before Mod_Cell_SetVol(0,value) and that lower result
is not propagated. The paired speaker_out_gain_get (28c64), helper28590 case7,
reads that configuration field. It is **cached requested state**, not DSP or
acoustic readback. Even matching get/put values cannot certify audible output.

Existing mod_audio/get_volume likewise reports its accepted request cache. Public
comments now make that scope explicit; fake-HAL failure-gate tests remain valid
control-flow tests, not proof that real HAL errors reach them. Native integration
must keep requested/cache/transport/hardware acceptance distinct.

## Read-only real-TV query

New optional diagnostic tools/audio/mixer-probe.c, make build/mixer-probe or
TARGET=tv build-tv-v2/mixer-probe, opens hw:0 with SND_CTL_READONLY. It only queries
the known speaker-gain control's integer metadata/value; there is no setter.
It uses opaque ALSA objects and development headers on the build host. Strict
host and ARM C11 -Wall -Wextra -Werror -pedantic builds pass; the ARM diagnostic
links the target's fixed libasound.so.2 and was actually executed on the TV.

First execution failed because a fresh SSH environment lacked the stock ALSA
configuration variable. The library tried its original build-host path; this
was not an absent audio device or a failed getter. The target's /vendor/run.sh
line254 exports ALSA_CONFIG_PATH=$PWD/etc/alsa.conf while running from /vendor.
With the same per-process environment, the unchanged binary passed:

    ALSA_CONFIG_PATH=/vendor/etc/alsa.conf mixer-probe --read-gain
    card=0 control=speaker out gain count=1 min=0 max=100 values=0

Exit0. Test binary **23,332 bytes**, MD5 **397f724bd60ad5292bfb494e7225c24d**;
upload and size/hash verification occurred separately before execution. This
confirms runtime access to the driver-cache getter; it is not a gain-write or
independent native bring-up test. --check verifies loader closure, not ALSA
configuration resources or audio health. Do not blindly run the full platform/
audio initialization while stock owns hardware just to prepare this query.

Staging /tmp/oa-mixer-20261101 and helper removed. Same boot
ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 and UI PID1737; Ethernet, SSH/telnet,
hosts/Samba protection and hook/keeper hashes verified afterward. No persistent
deployment, boot/recovery change, source/key/volume/mute/reset/table change.

Evidence: ../analysis/cec-ownership-2026-11-01/audio-gain-*, live-audio-mixer-*,
live-audio-kitems*, live-mixer-*. DumpFirmware.java now supports bounded
data:hex-address:length reads without modifying a project; it was exercised on
the actual dispatch table. Existing scalar/failure/cache tests were rerun and
pass after comment corrections. Next trace dispatcher completion plus checked
gain/table application and keep audible/lifecycle acceptance separate.
