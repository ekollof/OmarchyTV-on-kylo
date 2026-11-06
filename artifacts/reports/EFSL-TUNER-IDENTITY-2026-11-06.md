# EFSL profile decode and tuner identity — 2026-11-06

Scope: recover the profile-selected tuner type that blocked `mod_tuner`, and
implement the read-only container/selection logic with tests. No tuner IIC,
display, service or state change was performed on the TV. The only TV access
was read-only `md5sum`/`ls` of `/conf/mb230_{hw,sw}profile.bin`; the set stayed
in stock operation.

## Sources

| Asset | Identity |
|---|---|
| `aurora.elf` | MD5 `64c252340d3af6c5d66072611b7bad7a` |
| `mb230_hwprofile.bin` | MD5 `1b063b2cc7faa8b92c0f843921e561c7`, 24514 bytes |
| `mb230_swprofile.bin` | MD5 `a1d9e5e80a926a6e0abd812c98852894`, 60075 bytes |

The live TV `/conf` copies matched both MD5 values on 2026-11-06, so the
artifacts below are the runtime profiles, not stand-ins. Ghidra project:
`artifacts/workbench/ghidra_proj` (`Aurora` / `aurora.elf`), read-only.

## EFSL container

Recovered methods:

- `ROMProfile::initialize` @ `0x00c5dd44`
- `ROMProfile::getUniqueID` @ `0x00c5e100`
- `ROMProfile::findValue` @ `0x00c5e298`
- `ROMProfile::readValue(variant,key,char*,uint)` @ `0x00c5e350`
- `ROMProfile::openSection(variant, name)` @ `0x00c5e1fc`
- file loader `ICE_LoadProfileData` @ `0x007a34a8` (`%s/%s%s.bin`)

Header/table layout (little-endian):

```text
0x00  "EFSL"
0x10  profile name, 16 bytes ("hwprofile"/"swprofile")
0x20  base word; ROMProfile::initialize only publishes a base when this is 0
0x30  root section: +0x08 total entry count, entries start at +0x14
0x38  root count
```

Each entry is 20 bytes:

```text
+0x00  key hash
+0x04  type: 1 subsection, 2/3/4 value
+0x08  subsection child count, or value length
+0x0c  value offset from file base
+0x10  unused by findValue/readValue
```

`findValue` walks entries from `section+0x14`; a type-1 entry advances by
`1 + child_count` entries. `readValue` accepts only types 2..4, requires the
stored length to be no larger than the caller length, then copies that length
from `base + value_offset`.

### Key hash

`getUniqueID` seeds `0xf1d3b597 ^ (strlen(key) << 6)` and, per byte:

```text
v = (h + (~prev << 1)) ^ (sum << 9)
v = v + (c << 17)
v = v ^ w
w = c + (w << 8)
v = v + (v << (c % 3))     // __aeabi_uidivmod remainder
v = v + (v >> (prev % 5))
```

`c` is sign-extended (`ldrsb.w`). `0x0128570c` was verified to be
`__aeabi_uidivmod` (quotient r0, remainder r1), not a plain divide.

Pinned hashes (and where they occur in the shipped files):

| Key | Hash | Occurrence |
|---|---|---|
| `TunerType` | `0x04461a6a` | hwprofile `0x57ec` |
| `TunerTypes` | `0xc4791570` | hwprofile `0x5788` |
| `Version` | `0x74a04762` | hwprofile `0x21c8`, swprofile `0xe9f0` |
| `SwProfileVersion` | `0x9af10992` | swprofile `0xe9dc` |
| `Si2151` | `0x3e73b5e9` | hwprofile `0x579c` |
| `Si2157` | `0x3e3035e9` | hwprofile `0x57c4` |
| `Si2158` | `0x079a6f30` | hwprofile `0x57d8` |

Parsed sanity values: swprofile `SwProfileVersion/Version` = `"39"`;
hwprofile `TunerTypes` has five children and `TunerType` = `"Si2157"` (7
bytes including NUL).

## Tuner selection

`Profile_GetTunerType` @ `0x00c6398c` reads profile `"hwprofile"`, section
`"TunerTypes"`, key `"TunerType"` into a 20-byte buffer.

`SAL_DEMOD_TUN_GetInitTunerType` @ `0x0084adb8` calls `FUN_0084abb8` while the
cached type is `0xf`. `FUN_0084abb8` @ `0x0084abb8`:

```text
layout = DEV_COMMON_GET_PCBLAYOUTTYPE()      // global at 0x0175f028
switch (layout) {                            // tbb at 0x0084abe0:
  3,4,5,6 -> 0xf                             //  bytes 03 03 03 03 11 27
  8       -> 8                               //  layout 7 -> default,
  default -> Profile_GetTunerType(buf)       //  layout 8 -> type 8
             strcmp(buf, "Si2151")
             equal -> 8, otherwise -> 0xd
}
```

`DEV_INI_Init` @ `0x00853a58`, called from `ICE_HardwareInit`, writes 14 to
the layout global; the global's `.data` initializer is 2. The only other
writer, `DEV_INI_BY_PCB_Layout_Setting_Init`, has no in-binary caller (its
symbol is only an export). Layout 14 therefore takes the profile branch.

The hwprofile value is `"Si2157"`, not `"Si2151"`. The shipped binary contains
no `"Si2157"` string; the only tuner compare literal is `"Si2151"` at
`0x01355ccc`. The instructions are a `strcmp` (`0x006b4c48` = `strcmp@plt`):
`"Si2157" != "Si2151"`, so the firmware selects SAL type **0xd (MxL661)**.

This is a vendor behavior/quirk of this profile, not a guessed chip identity.
Keep it for bug-for-bug parity unless the owner explicitly chooses otherwise.

`_convertSalTunerType2NdNimTunerType` @ `0x0084ad44` maps SAL type to the
NIM HAL type via the 14-entry table at `0x01355c74`:

```text
0..13 -> 8,8,8,0xc,8,5,6,5,8,8,8,8,8,0xc
>0xd  -> 8
```

So SAL 0xd -> NdNim 0xc; SAL 8 -> NdNim 8.

## Implementation

- `common/efsl_profile.c`, `include/openaurora/efsl_profile.h`: read-only
  EFSL open/root/section/find/read with the recovered hash and entry walk.
  Every count, offset and copy is validated against the supplied buffer
  before use (the vendor trusts the file; replacement input must not).
- `common/tuner_identity.c`, `include/openaurora/tuner_identity.h`:
  `oa_tuner_sal_type(layout, hwprofile)` reproduces `FUN_0084abb8`, and
  `oa_tuner_ndnim_type()` reproduces the conversion table.
- Failure policy deviation: when the profile value is missing or unreadable,
  the vendor passes an uninitialized stack buffer to `strcmp`; the result is
  undefined. The implementation deterministically returns 0xd and documents
  this instead of modeling garbage.

## Evidence

```text
$ make check-efsl
ok efsl profile: hash/entry layout, readValue bounds, tuner identity and NdNim mapping

$ make check-efsl-corpus          # MD5-pinned hwprofile+swprofile
ok efsl profile: hash/entry layout, readValue bounds, tuner identity and NdNim mapping

ASan+UBSan build/run (same corpus): ok
Full suite: all tests passed
ARM musl static build, qemu-arm-static, same corpus: ok
```

Tests cover known hash vectors, real table counts (1214 hw / 2992 sw), the
`"Si2157"` value, `"Si2151"`->8 vs `"Si2157"`->0xd, layout 3..6/8 overrides,
the full NdNim table, bad magic, nonzero base word, truncated/count-overflow,
out-of-range value, oversized value, child-overrun and section/value type
mismatch.

## Remaining

`mod_tuner` is still deliberately unwired. The next hardware-side pieces are:

1. `SAL_TUNER_MxL661_Init` @ `0x00872968`: six
   `MxLWare601_API_ConfigDevice/ConfigTuner` commands
   (soft reset 0, overwrite-default 1, xtal 2, IF-out 4, AGC 0xc, power-up 0xa)
   at I2C address 0xc0, with the register sequences and failure behavior.
2. `HAL_TUNER_Init` @ `0x0084c164`: four bus IDs from
   `DEV_IIC_GetDescription(0..3)` byte +0x19, globals `0x175eb78` (tuner),
   `0x175eb76` (sat), `0x175eb77` (ext demod), `0x175eb75` (LNB), default 0.
3. `HAL_TUNER_ReadBase/WriteBase` @ `0x0084c364`/`0x0084c304`:
   `Hal_Iic_Read/Write({bus,reg}, value, len, buf)`, success is `rc == 0`.
4. `HAL_Demod_Init(0)`, `_HAL_dvif_open`, `HAL_DEMOD_EXT_DVIF_SetTunerType`,
   AV2018 response handling and the RT5047 LNB path.

Only after a real backend exists and passes fake-device tests should the
NIM-DTV chain be wired opt-in into `mod_tuner`. No tuner hardware has been
accessed.
