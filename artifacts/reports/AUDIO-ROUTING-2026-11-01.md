# Audio routing and actual amplifier — 2026-11-01

Status: firmware and read-only live-state confirmation, plus an offline snapshot
decoder. No audio write, playback test, takeover, restart, persistent deployment
or recovery change. This advances the still-incomplete audio/CEC parity work.

## Correct amplifier selection

The earlier mod_audio comment claiming AD82586C was wrong. Aurora includes
several amplifier drivers; presence of a symbol/config filename is not selection.
Using the verified Aurora MD5 64c252340d3af6c5d66072611b7bad7a:

- DEV_AMP_GetDesc (84f5ac) returns 0192fa5c. GetAmpCount (84f5e4) reads its
  first word. GetAmp (84f620) checks index against that count and reads the pointer
  at +4+4*index. Ghidra's saved void return masks this in its decompilation;
  ARM instructions supply the actual behavior.
- Live descriptor: count **1**, first pointer **033341c0**. The next words (72/72)
  are outside that one-element result; they are not additional amplifier pointers.
- The 216-byte object has id/type word +8 = **1**, size +12 = d8 and name at +16
  **ESMT AD82587_D**. Its callbacks +44=874ef1, +4c=875599, +70=874e51,
  +80=8752a9 agree with AMP_AD82587_Create (8755d0): Init, Open, Mute, SetVolume.
  Callback addresses retain the Thumb bit. AD82586C_VESTEL's different id 13 and
  function addresses do not match the live object.

AMP_AD82587_Init (874ef0) applies profile-derived GPIO/reset/mute sequencing,
delays, prescale/channel/fine-tune/DRC parameters and 38 I2C register writes.
It also invokes optional subwoofer helpers. Those helpers' AD82586C names do
not change the main amplifier identity. Profile/GPIO polarity and board control
transport must be recovered before native bring-up; no generic I2C scan/write
or reset sequence was attempted.

AMP_AD82587_SetVolume (8752a8) writes register 3 with the parameter's low byte
unless its private flag suppresses writing, then reads three bytes beginning at
3 and checks the returned first byte. This is not evidence that its operand is
the UI's 0..100 volume scale. Recover the caller's conversion/table before reuse.
DEV_AMP_SetVolume/Mute/Open are vtable dispatch wrappers, not reusable HAL exports.

## AOUT records and distinct controls

PLAT_AOUT_Init (88744c) initializes three 24-byte outputs at 0197a138 followed by
nine 60-byte speaker records at 0197a180. Connect (888498), SetSpeakerVolume
(889680) and SetSpeakerHWMute (888fe0), with ARM instruction checks, establish:

| record | recovered fields |
|---|---|
| output | id +0, audio handle +12, linked speaker head +20 |
| speaker | previous/next +0/+4, id +8, output id +12, present +16, HW mute +18, left/right uint16 gain +24/+26 |

The initial output id 7fff means disconnected. Pointer fields are addresses in
the vendor process; they must never be dereferenced as host/native pointers.
Output 0's live handle **01234567** is the observed token, not an independently
opened native resource or proof that an output is physically audible.

The volume helper at 887154 uses the speaker id and the **left** gain:

| speaker id | operation |
|---|---|
| 0 | Hal_Audio_EasySet item 42 |
| 1 | item 45, trace names HPLINEOUT_GAIN |
| 2 | item 4f, trace names SCART_OUT_GAIN |
| 3 | item 4a, trace names SPDIF_OUT_GAIN |

All pass a pointer to a 32-bit value and third argument zero. Other ids follow a
success path without one of those writes. Most branches ignore the HAL result;
even speaker 0's error path returns success. Do not copy false success as native
hardware verification. NDAL3 Hal_Audio_EasySet at 4e7aea34 packs its three input
words for Hal_Audio_SetControl; no live HAL call was made.

SetSpeakerHWMute sends Hal_Audio_Ctrl(35) with the **record's speaker id**, not
its output id, plus the cached mute byte. Existing mod_audio's id 0 is a registered
speaker on this set, but this alone does not establish complete amplifier mute.
Its volume opcode 33 is decoder main volume. AOUT gain, amp register volume and
decoder volume are separate layers. Keep external CEC volume/status separate too.

## Read-only confirmation and decoder

Live cache sample, 612 bytes, preserved as live-audio-routing.bin alongside its
source hex log. Output 0 has the only nonzero handle/list; outputs 1/2 have neither.
Present speakers 0,1,2,3,5 all connect to output 0. Speakers 4,6,7,8 are absent
and have disconnected output id 32767. All HW-mute bytes are zero. Cached L/R:
0→0/0, 1→20/20, 2→0/0, 3→100/100, 5→100/100. These are sampled cached gains,
not the UI volume, measured loudness or a complete physical routing map.

New openaurora/tools/audio/audio-state.c (make build/audio-state) decodes the
snapshot offline, with explicit little-endian fields and exact size/all-record
identity checks before printing. Host, ASan/UBSan and ARM/qemu outputs match.
Short/long inputs and incorrect record ids reject without partial stdout.
The tool does not contact the TV or modify its input. A live snapshot is not atomic.

The selected live UI remained PID1737, sleeping, and boot ID
ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 remained unchanged. Only /proc memory reads
were added after the prior CEC cleanup; no helper was uploaded or left running.
Production core/modules and installed/recovery files are unchanged. mod_audio
comments were corrected; its executable behavior was not changed.

## Remaining integration and avoided detours

CEC's retained failed-close session cannot yet be safely handed to the current
void module stop hook. A lifecycle mechanism that can preserve ownership across
failed cleanup, plus owned blocking-I/O workers, remains necessary before a CEC
module can safely reload. Do not hide this by discarding a failed-close session.

For audio, next recover selected AD82587 board resources, GPIO and I2C ownership,
the UI-volume conversion/callers and paired init/close/resume before implementing
and accepting native audio. The AD82586C path is a sibling driver, not the next
bring-up target. dmp_get_audio_type in amp-dispatch concerns DLNA content, not
board amp type. DumpCallers finding no direct DEV_AMP_SetVolume references does
not mean it is unused: indirect/PLT dispatch needs separate tracing. The final
query @84ebf0 in amp-factory failed because Ghidra has no function there; earlier
results remain valid. amp-task-instructions is objdump's actual Thumb code at
the task entry passed by DEV_AMP_Init, not the misleading nearest-symbol label.

Evidence: ../analysis/cec-ownership-2026-11-01/amp-*, ad82587-contract,
audio-easyset-abi, live-audio-*, live-amp-* and audio-state-*.
