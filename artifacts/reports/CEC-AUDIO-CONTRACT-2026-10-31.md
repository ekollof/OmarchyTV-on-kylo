# CEC and audio contracts — 2026-10-31

Status: current offline instruction/specification evidence and tested foundations.
No TV access, CEC transmission, sound change, deployment or restart occurred.
Owner priority is now CEC/audio. Follow the applicable CEC specification; vendor
quirks need explicit review rather than automatic reproduction of violations.

## Sources and corrections

Local specification copies now live in artifacts/workbench:

- cec13a.pdf / cec13a.txt: HDMI 1.3a Supplement 1, including CEC 12.3, 13.15,
  System Audio message table and Audio Status operands. PDF MD5
  f276f28004377aff85c3af1bcae4331a.
- cec14a.pdf / cec14a.txt: HDMI 1.4 specification, CEC 13.17.2.1/2 and Table 25
  for ARC, base 8.7.3 for physical-address allocation. PDF MD5
  db94e71df404736500f529f1a895cac1. Downloaded from
  https://felloff.net/doc/comp/spec/HDMISpecification14a.pdf; document labels itself
  Version 1.4. Do not infer the revision from the download filename.
- This TV's Aurora MD5 64c252340d3af6c5d66072611b7bad7a and libNDAL3 MD5
  c842bad804302ec822076c3d9d3ce31d. Ghidra instructions are preserved in
  ../analysis/cec-audio-2026-10-31.

The old CEC-SPEC report incorrectly reverses ARC initiation and overstates
conformance. The Audio System is the ARC receiver: it sends Initiate ARC (C0)
to the TV, the ARC transmitter. The TV replies Report ARC Initiated (C1) or
Report ARC Terminated (C2), or aborts as applicable. The TV may request initiation
with C3. C4 requests termination; the Audio System sends Terminate ARC (C5).
These are directed messages. Adjacency/capability checks and hardware activation
are separate requirements; correct opcode direction alone does not establish ARC.

While System Audio Mode is on, volume controls target the external amplifier;
the TV/source must not change its internal volume. Incoming amplifier status
belongs to external-volume UI state, not a local speaker-volume setter. Audio
Status bit 7 is mute; bits 6..0 are 0..100 percent, 127 unknown, 101..126 reserved.
Never clamp 127 to 100. Set System Audio Mode can be directed or broadcast;
System Audio Mode Status and Report Audio Status are directed.

Unknown broadcast commands must not elicit Feature Abort. The old statement
that *all* directed replies to broadcasts are forbidden was too broad. Direct
unsupported messages require the appropriate abort handling; silent ignore is
not the generic default. A policy engine still needs opcode-specific handling.

## Implemented foundations

`openaurora/common/cec.c` and public cec.h implement bounded 1..16-byte frame
encoding/decoding, distinguishing header-only polling from opcode zero. Helpers
validate the covered System Audio messages, volume up/down/mute key subset,
audio-status values, physical-address tree structure and six ARC directions for
the TV(0)/Audio System(5) profile. Unsupported is distinct from malformed and
does not automatically request an abort. No electrical timing, adapter ownership,
or retries exist here. Automatic System Audio/ARC replies and the cec.audio
service were added later in OWNER-WORKER; this section remains the
transport-independent foundation.

Audio instruction recheck corrected the shared vendor ABI in vendor_audio.h:

- Hal_Audio_Protect_Init (4e7aeeb0): no semantic argument; scalar 0/-1 return.
  The entry r0 store is stack probing, not evidence of an enable argument.
- Hal_Audio_Mixer_Ctrl_Init (4e7ad940): returns scalar status via saved local
  at stack +0x1c; success/already-initialized paths return zero, failure -1.
- Hal_Audio_Proxy_Init (4e7af62c): returns the status retained in r4. Its vendor
  open-fd test is imperfect; zero return is not proof of device health.
- Hal_Audio_Ctrl (4e7af6cc): common exit moves r6 to r0 (4e7af7ae); null params
  return -1. Opcode 33 uses Hal_Audio_Send with a 56-byte block. Opcode 35
  reads/updates the hardware mute mask through EasyGet/EasySet and returns status.
  It ignores the EasyGet failure internally, so even zero needs hardware acceptance.

mod_plat owns Protect/Mixer/Proxy initialization; it now checks their statuses
before later platform stages. mod_audio no longer repeats the prelude on each
load/reload. It holds plat, checks readiness/errors, preserves a peer service on
publication collision, propagates Ctrl failures and only updates cache on success.
Snapshot restore validates both bytes atomically and reapplies known volume/mute;
255 marks unknown and never causes a guessed zero setting. HAL acceptance is
not audible-output proof. Applying volume then failing mute can leave a partial
hardware change; no transactional rollback is claimed.

## Validation and next work

`make BUILD=build-strict check-cec-audio` runs nine regression groups: frame
round trips/bounds, all 256 audio-status bytes, System Audio/key golden frames,
ARC direction/PA rules, audio requests/errors, handoff/invalid snapshots, unknown
cache, publication/platform/disabled cleanup, and platform-prelude failure gates.
They pass strict C11 -Wall -Wextra -Werror -pedantic, ASan/UBSan and ARM/qemu.
The full host suite also passes. HAL entries are local fakes, never TV devices.

Next recover transport below PLAT_CEC_SendCMD (88c2bc), Open (88be98), and
Get_Validated_CEC_Command (88c0c4), including private helper 88bb4c, MCU packet
ownership, thread/receive framing and close/resume. No Linux /dev/cec API is
assumed on this vendor kernel. The injected System Audio/ARC route and the
local-volume refusal now live in OWNER-WORKER. Native transport, electrical ARC
and UI routing remain open. The old workarounds/destination rewriting must be
checked against the spec.

Local speaker output id 0 remains an unverified candidate: the vendor
PLAT_AOUT_SetSpeakerHWMute (888fe0) looks up the actual hardware speaker id from
its routing table before opcode 35. Recover amp/board routing, profile settings,
volume curve, init/close/resume and independent audible behavior before acceptance.
Platform restart after partially initialized vendor globals remains unresolved.
