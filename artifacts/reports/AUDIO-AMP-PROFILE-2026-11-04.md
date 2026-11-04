# Amplifier inputs from the shipped hardware profile — 2026-11-04

Follow-up [AMP-PROFILE-MODULE](AMP-PROFILE-MODULE-2026-11-04.md) implements
the reloadable data provider and CLI/actual Unix IPC, with host/sanitizer/ARM
lifecycle tests. Provider absence below describes this decoder's earlier
milestone; audio consumer and native image/hardware ownership remain open.

`common/audio_amp_profile.c` now supplies a fresh, caller-owned AD82587 input
snapshot from bounded hwprofile bytes. This connects the actual profile data
to the register-image helper. It does not operate hardware or supply a module/
worker lifecycle. All work was offline; the owner's TV restriction remains.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only Ghidra decompilation
and ARM instructions cover Profile_GetMainSpeakerPower@c5e858, the four amp
getters@c5f090/c5f1f8/c5f2ac/c5f144, Profile_GetDRC@c5f6f0, ROMProfile's
byte-copy overload@c5e350 and initialize@c5dd44. The concrete literals are
resolved from ELF load segments in metadata.txt, not inferred from API names.

- Profile `hwprofile`, section `AudioSettings/Prescales`, key `DefaultSpeaker`.
  The byte-copy path accepts types2..4 and length<=30, including the stored
  terminator. If that path fails, the numeric reader defaults to255. Indices
  0..4 map to8Watt,6Watt,2.5Watt,12Watt,SoundBox4R12Watt.255 returns empty.
- Empty power uses the8Watt section for numeric lookup, while the returned
  power name stays empty. The four keys under `AudioSettings/Prescales/%s` are
  Amp_Prescale, Amp_Ch1_Volume, Amp_Ch2_Volume and AmpVolumeFineTune.
  **All four defaults are c0**, established by the fifth stack argument.
- The five DRC keys use `AudioSettings/Prescales/%s/DRC`, each defaulting to
  ffffffff. Signed getters retain the exact32-bit result for the image policy.
- ROMProfile initialize sets object+c to the file base when header+0x20 is0.
  Byte records use an absolute offset at+c and byte count at+8. This is not an
  inline string or an offset relative to the record/string-table header.
- Scalar getters cache while their static value is not-1; power has its own
  nonempty string cache. The new function makes a fresh immutable snapshot
  instead of sharing Aurora globals. Its caller owns the32-byte power buffer
  and retains the snapshot until explicit reload. No module provider exists yet.

The shared `oa_profile_data` reader reuses bounded hash/section traversal. It
borrows the first matching type2..4 payload with its exact length, adds no NUL,
and validates its range against the actual supplied bytes. Wrong type/missing
returns absence; errors preserve outputs. A matched payload using an external
data base (nonzero header+0x20) returns ENOTSUP, not a guessed pointer.

The amp reader copies into caller storage, resolves legacy enums and reads the
numeric inputs/defaults atomically. Oversized byte values fail the firmware's
30-byte copy path and fall back to numeric/default behavior. Nonterminated
accepted-length strings reject EBADMSG; invalid enum indices reject ERANGE.
These bounded malformed-input policies intentionally avoid the vendor's unchecked
strcpy/table access; they are not claims of vendor fault parity. Inputs/default
values are not UI volume or a substitute for physical gain/ownership.

## Saved target profile and tests

Local mb230_hwprofile.bin is24514 bytes, MD5
`1b063b2cc7faa8b92c0f843921e561c7`. It yields power12Watt, prescale1b,
ch1/ch2=05/05, fine80, clip0630e8, SpeakerDRC018000, state/attack/release=-1.
The1b/05/05 fields agree with AUDIO-BOARD's earlier register03..05 sample;
this is consistency with dated evidence, not a fresh TV query or audible proof.
Advertised total size includes bytes absent from this saved profile; the reader
uses actual ranges and does not pad/reconstruct a guessed larger file.

- Strict host profile/amp tests pass, including the real saved profile.
- Focused ASan/UBSan with leak detection and fresh ARM/qemu tests pass. Cases
  cover strings, all five legacy enums, empty/oversized power fallback, numeric
  defaults, register-image join, truncations, payload overflow, external-base/
  nontermination/invalid-enum rejection, wrong-type duplicate precedence,
  borrowed payloads and atomic output errors.
- Native ROMProfile byte-copy instructions match **2312** type/length/capacity
  cases, with full destination/guard comparison. The existing hash/scalar,
  AD82587 image and all prior groups also pass: **44 groups total**. Only stack
  guard literals and known strlen/memcpy PLT callees are redirected in this
  payload fixture. No original getter cache or hardware init is started.
- Full strict host suite passed (356 ok lines). The final stronger edge cases
  also pass the focused host/sanitizer/ARM tests.

Evidence: `artifacts/analysis/audio-amp-profile-2026-11-04/`. getters.txt,
string-reader.txt, rom-init.txt and metadata.txt establish source/defaults;
host.txt/full-host.txt, build-0/1.txt, final-0/1/2.txt and native-reference.txt
record execution. The reference log identifies the executable SHA256 and every
completed group. Build/reproduce with TARGET=tv BUILD=build-tv-v3/amp-profile
and the maintained compiler as in AUDIO-AMP-NATIVE, then invoke verify.cjs.
Its default now names this build and requires the payload group.

Still open: reloadable provider/consumer integration and complete native getter
cache/history equivalence; retained baseline acquisition/ownership; GPIO/MCU/I2C
and worker lifecycle, startup/standby/resume and audible acceptance. No broad
audio or amplifier lifecycle TODO is closed by this data decoder.
