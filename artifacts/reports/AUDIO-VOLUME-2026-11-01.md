# Vendor volume path and AQ curve implementation — 2026-11-01

Status: UI-to-speaker path traced; bounded AQ curve decoder implemented and
tested on host, sanitizer, ARM/qemu and real TV. Not yet integrated into mod_audio
or a complete native gain/routing/amp lifecycle. No audio setting was changed.
Follow-up AUDIO-CURVE-MODULE-2026-11-01 integrates this decoder into a reloadable
data provider; hardware application/full native audio remain unfinished.

## Recovered path and corrections

ICE_AudioVolume (7df2ec) clamps the unsigned UI level to100, updates state and
calls _ice_Sound_ControlSetBalance (78e0a8). That path considers digital output,
DAP/headphone/output selection and invokes PLAT_VAUD_SetSpeakerVolumeBalance
(8d1c3c). It also has source/mute-specific behavior, DRC/DB/DAP changes and
telemetry calls. **Samba telemetry calls remain excluded from openaurora.**

SetSpeakerVolumeBalance clamps level to100 and balance to[-50,50], converts
balance to0..100, applies speaker balance and compares cached left gain before
setting equal left/right gains. PLAT_AOUT_SetSpeakerVolume then uses the speaker
gain helper described in AUDIO-ROUTING: speaker0 → Hal_Audio_EasySet item42,
headphone/scart/SPDIF →45/4f/4a. These levels select a separately configured
101-entry DSP gain table. This is not evidence of a direct percentage-to-AD82587
register03 mapping. The amplifier register path and decoder-main-volume opcode33
remain separate; no new conversion to amp register values was invented.

PLAT_AOUT_SetMainVolume (8d1e80) is a misleading shortcut: instructions only check
r1<=100 and return1/0, with no hardware write. Do not use its name/success return
as proof of volume control. Ghidra's saved void return hides that range check.

## AQ layout and exact decoding

PLAT_VAUD_ReadAQFile (8d2c34) requires exactly **0x684 /1668 bytes** and reads
the complete file into the AQ object. It does not skip the text header.
InitAQSetting (8d2dd4) selects:

| Profile_GetVolumeCurve result | table byte offset in AQ.bin |
|---|---|
| 1 | 1b8, aggressive |
| every other value | 24, normal |

Each table is101 little-endian 32-bit entries. PLAT_VAUD_SetVolumeTable
(8d29f0) performs101 word loads and **halfword stores** into a202-byte allocation
at the pointer stored in01981338, intentionally narrowing to low16 bits. It then
calls PLAT_AOUT_VolTableConfig (8875e8), which emits Hal_Audio_Ctrl2e (table
pointer/size101) and3e (table selection). These are recovered contracts, not new
live HAL calls. PLAT_VAUD_Init (8d4588) has additional path/routing/tasks/DAC/AQ
processing and a compiled default fallback at0176da84 if AQ initialization fails.
The new decoder does not claim to implement those stages or fallback policy.

Implemented common/audio_curve.c and public audio_curve.h:

- oa_audio_curve_from_aq requires exact size, uses explicit little-endian reads,
  selects the firmware offsets/branch and narrows each entry identically.
  Null/wrong-size inputs reject without changing output; unaligned input works.
- oa_audio_curve_value returns the selected raw16-bit entry and clamps unsigned
  levels above100, with null guards. No invented logarithmic formula, percent
  conversion, amplifier gain conversion or device access.

The helper is transport-independent data code available for native integration;
it is not yet wired into the production audio module. Existing mod_audio controls
decoder main volume and one speaker HW-mute operation, so complete UI parity
still requires routing, gain application, profile/default handling and lifecycle.

## Independent live evidence and validation

Dump AQ.bin and live /conf/AQ.bin both MD5 **0a56db61185c1c921beb264e964a6eb7**,
1668 bytes. Source Aurora MD5 remains64c252340d3af6c5d66072611b7bad7a.
The pointer at01981338 was **03323270**;202 bytes were read without modification
from the verified live process1737. AQ pointer0197f9b8 was03296e00. The cached
ICE volume/mute block at018e9e10 contained zeros at that sample; it is not a
calibration of loudness or proof of every current output policy.

All101 normal-curve values from the decoder match the independently captured
runtime table. Examples: [0]=0, [1]=5, [50]=8137, [100]=32768. This proves equality
to that normal table; it does not by itself identify the profile selector if
another curve can contain equal data. Raw capture is live-volume-table.bin,
MD5 **9b17a31c6bb3e58fdafbae4dcbf57b94**. No new device request was needed.

New tests/audio_curve.c / make check-audio-curve cover both101-entry layouts,
profile values including integer extremes, unaligned reads/high-word narrowing,
all1668 truncated lengths, oversized/null inputs and unchanged error output,
all visible volume indices and unsigned clamping. Optional AQ/capture arguments
compare every real table entry. No firmware table is embedded in source/tests.
Target is included in make check. Strict C11 -Wall -Wextra -Werror -pedantic,
ASan/UBSan and ARM/qemu pass, including the real-file comparison.

The ARM test ran on the TV from temporary staging, reading /conf/AQ.bin and the
captured table. It passed all three groups, exit0. Test size26,976 bytes, MD5
**17e9b4f7a43915f8331c74f310db7eed**. Uploads and size/hash verification were
separate successful connections. No HAL initialization, gain/mute/routing/reset
operation, key, source or audio playback change occurred.

Staging /tmp/oa-curve-20261101 and its files were removed, no test process left.
Boot ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 and vendor PID1737 unchanged; Ethernet,
SSH/telnet, hosts/Samba blocks and hook/keeper hashes verified afterward.
Production core/modules, installed files and recovery remain unchanged.

Evidence: ../analysis/cec-ownership-2026-11-01/volume-*, aq-file-layout,
live-volume-*, live-aq-pointer-volume and live-curve-*. Continue with owned native
gain application and amplifier lifecycle; full UI volume/mute/source/CEC System
Audio behavior still needs integration and audible acceptance.
