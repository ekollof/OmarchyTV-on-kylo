# Source switch, mute, and key actions — 2026-11-08

## Independent verification (2026-11-08, same day)

The libNDAL3 claims used for implementation were re-read from the shipped ARM
library; the aurora jump table was re-checked in `artifacts/workbench/aurora.dis`:

- `Hal_Vin_Open` `0x4e7c97e8` formats `/dev/video%i` from a signed-byte table
  at `0x4e7d9740`: index 0..4 -> `20,24,21,28,66`. The open is `open64(path,
  0x802,0)` then `fcntl(fd,F_SETFD,FD_CLOEXEC)`, and `fd < 0` returns `-1`.
- `Hal_Vin_SelectInput` `0x4e7c993c` is `ioctl(fd,0xc0045627,&id)`;
  `Hal_Vin_SetStreamStart` `0x4e7c9d58` is `ioctl(fd,0x40045612,&one)`.
- `plat_vin_slot_connect` `0x0092ee24` re-checked: the `tbh` at `0x0092ee80`
  dispatches types 0/4/5/20/21 to `Hal_Vin_Open(0)`, 10 to `Open(1)`,
  14..18 to `Open(3)` then `Open(2)`, 19 to `Open(4)`, and 1/2/3/6..9/11..13
  to the fail path; the type-19 path at `0x0092eeb0` was read in full.
- `0x0092e8ac` sends `Hal_Video_Vpltio_SendCmd(0x4c4,0,&{tableA,tableB},0...)`
  then `Hal_Vin_SelectInput(fd,tableA)`, matching the report.
- The V4L2 wrappers are 4-word table copies (`Hal_Video_Get/Set_V4l2Info`,
  `0x4e7c3e4c`/`0x4e7c3d8c`) and the FdCommon wrappers; the VPLT ioctl is
  `0xc0245604` over a 9-word `{cmd,...}` struct (`0x4e7c85fc`).

Not verified/implemented: `PLAT_VIU_ChangeInput`'s full DRM/NDAL sequence,
libNDAL3 `Hal_Video_Drm_Set_DpSource`/`NDAL_VideoExt_OpenDevice` bodies, and
the actual display effect of the recovered VIN path. The mute analysis remains
instruction-read but the optical no-op is the stock external-output policy.

Evidence is the instruction listing in `artifacts/workbench/aurora.dis`
(`objdump -d --no-show-raw-insn`) and ELF `PT_LOAD` bytes of
`artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf`, MD5
`64c252340d3af6c5d66072611b7bad7a`. Thumb entries below are the symbol
address (low bit clear). A `tbh` pool is data; targets were decoded from
the ELF, base = the `tbh` address. No TV access. libNDAL3 was not opened,
so HAL ioctl layouts and the reason `Hal_Audio_Ctrl` returns `-1` are not
recovered. Ghidra was not re-run; the instructions are the contract.

Dynamic calls are `ldr` of the vtable, `ldr` of slot N from that vtable,
`blx`. Slot numbers are from the object vptr (`_ZTV` + 8).

## Source switching

### UI entry

`InputManager::selectExtSource(int, bool)` at `0x00fb1290` calls
`EASITVControlInterface::selectExtSource` at `0x00a23fd8`, which calls
`ExternalAVSourceManager::selectExtSource(int, bool)` at `0x00a2668c`.

At `0x00a266ac` the bool in r2 is saved in `sl` and flipped:
`eor.w r6, r2, #1`. The manager object at `this+0x24` is then called at
vtable `+0x34` (`0x00a26714`) with r1 = that flipped bool and r2 =
`ICE_ExternalGetVideoDevice(index)` (`0x007f30e0`). The class of
`this+0x24` is not recovered, so this virtual is not proved to be
`ICE_VideoSelectSource` or `PLAT_VIU_ChangeInput`. If that call returns
0, the audio block below is skipped (`cbz` at `0x00a26728`).

Audio, only after a nonzero video-virtual return:

- Original bool nonzero (`0x00a26746`): `AudioIF` at `this+0x2c`
  (`0x00a5fa0c` `stopAudio(1)`, return ignored),
  `ICE_ExternalGetAudioDevice(index)` (`0x007f31b0`),
  `AudioIF::setTransportStreamSource` (`0x00a5f6e8`). If that returns
  nonzero, `startNonDVBAudio` (`0x00a5f888`).
- Original bool zero (`0x00a267bc`): `stopAudioDescription` on `this+0x2c`.
  If `this+0x30` is nonzero, `stopAudio(1)` and `stopAudioDescription` on
  it. Then `setADTransportStreamSource` on `this+0x2c`; a zero return
  skips the starts. Otherwise `startNonDVBADAudio`, and if `this+0x30`
  is set: `setTransportStreamSource`, `setADTransportStreamSource`,
  `startNonDVBAudio`, `startNonDVBADAudio`.

`ICE_VideoSelectSource` at `0x007d5a34` does call
`PLAT_VIU_ChangeInput` at `0x007d5e12`. Registers there: r0 is a byte
loaded from `[r5,#4]`, r1 is a word loaded from a table
(`[ip,#4]`). That is a separate caller from the virtual above.

Other direct `bl PLAT_VIU_ChangeInput` sites: `0x0074cb4e` (inside
`ICE_HardwareActiveStandby`), `0x007d2bec`, `0x007d31da`
(`icevid_avclip_initial`), and `0x00902ee0` / `0x0090316a` inside
`ChangeInput` itself.

### `PLAT_VIU_ChangeInput` (`0x00902ae4`)

r0 is kept in r4, r1 (source type) in r5. First it calls
`PLAT_VIU_NdVideoSetting` (`0x00901c28`) and, if the word at the returned
pointer equals the type, branches to `0x00902ec2` (that branch was not
read). The `plat_vin_slot_connect` call at `0x00902cfe` is
`plat_vin_slot_connect(0, type)`: r0 is the immediate 0, r1 is r5. Its
return is ignored. The call is skipped when any of these hold:

- the compare at `0x00902cc4` (`[sp-local+4] <= 1`, or that word `> 1`
  and `[sp-local+8] <= 5`; `[sp-local+8]` was stored as `type-4` at
  `0x00902b54`; the writer of the `+4` word was not isolated);
- r4 `> 1`, or type `== 25` (`0x00902cce`);
- type `== 1`, which goes to `0x00902f2a`: `PLAT_DEC_SdVideoConnect`,
  `plat_drm_connect_dpsource(r4, 1)`, `NDAL_VideoExt_OpenDevice(r4, 1)`,
  no `slot_connect`;
- type is 2 or 3 (`0x00902ce4`): skip `slot_connect` and the DRM
  connect, still `NDAL_VideoExt_OpenDevice(r4, type)`.

When the call is taken, the next instructions are
`plat_drm_connect_dpsource(r4, r6)` at `0x00902d08` and
`NDAL_VideoExt_OpenDevice(r4, type)` at `0x00902d10`. Types 14..18
(`sub r3, type, #14; cmp r3, #4; bhi` not taken) then
`plat_vin_slot_get_hdmi_phy` / `plat_vin_slot_set_hdmi_phy`, with a 1
stored at the phy buffer `+0x1c` before the set. Later, unless another
compare branches away: `PLAT_VIU_NdVideoSetting` again, `str type` at
the returned struct `+0`, `PLAT_SRCIN_GetVinParam(type)`, and on a
nonzero param pointer `PLAT_VIN_UpdateTDLayerInputSource`.
`GetVinParam == 0` prints and returns 0 (`0x00902f1a`).

`plat_vin_slot_connect` is also called from
`PLAT_Audio_Testcase_ExtInADC_Connect` (`0x008869da`) and
`PLAT_VIN_ConnectATV` (`0x008f49d4`). Those two call sites were not
disassembled.

### `plat_vin_slot_connect(slot, type)` (`0x0092ee24`)

Return is r0: 0, or `-1` (`0xffffffff`). The return is an explicit
`mov` before the stack-canary compare. It does not return through
`__stack_chk_fail`.

1. `pthread_mutex_lock(0x019e82a0)`.
2. Unnamed helper `0x0092e93c(type, 1)`. Its return is ignored.
   The helper has no global symbol (objdump labels it
   `plat_set_dimming+0x2d4`). If `type` is outside 4..21
   (`subs r3, r0, #4; cmp r3, #17; bhi`), it returns `-1` and does not
   call audio. Otherwise, because r1 is 1, it calls
   `Hal_Audio_EasySelect(r0=0, r1=1, r2=0x01773918)`. The bytes at
   `0x01773918` are the word `1` followed by the C string `adc on`.
   A nonzero EasySelect return is returned as-is. A zero return
   increments the word at `0x019e8584` and returns 0.
3. Slot record is 12 bytes at `0x019e852c + slot*12`. Word `+4` is the
   connected flag. If it is already 1, unlock `0x019e82a0` and return 0.
   `plat_vin_slot_init` (`0x0092e9c4`) writes the initial records,
   including type `25` at `+8` (`movs r6, #25; str` at `0x0092ea36`).
4. If `type > 21`, branch to `0x0092f0ce`: unlock the first mutex and
   return `-1`. No second mutex on that path.
5. Otherwise `tbh [pc, r5, lsl #1]` at `0x0092ee80`. Base is
   `0x0092ee84`. ELF halfwords:

| type | target | open index |
|---|---|---|
| 0, 4, 5, 20, 21 | `0x0092ef72` | `Hal_Vin_Open(0)` |
| 10 | `0x0092efe2` | `Hal_Vin_Open(1)` |
| 14, 15, 16, 17, 18 | `0x0092f056` | `Hal_Vin_Open(3)` then `Hal_Vin_Open(2)` |
| 19 | `0x0092eeb0` | `Hal_Vin_Open(4)` |
| 1, 2, 3, 6, 7, 8, 9, 11, 12, 13 | `0x0092f0ce` | fail, return `-1` |

`Hal_Vin_Open` `fd < 0` takes the fail path. The fail path sets r6 to
`-1` and still falls into the slot-record update when `slot <= 6`,
then unlocks `0x019e82a0`. Success sets r6 to 0 and updates the same
way. The record is `+0` byte = slot, `+4` word = 1, `+8` word = type.
The mutex at `0x019e82b8` is unlocked around the VIN work in this
function. The matching lock is not in this function.

Open(0), Open(1) and Open(4) then call `0x0092e8ac(fd, type)`,
`Hal_Vin_SetStreamStart(fd)`, `Hal_Video_Get_V4l2Info`, overwrite
words, `Hal_Video_Set_V4l2Info`, `Hal_Video_Get_V4l2FdCommon`, splice
the new fd into one word, `Hal_Video_Set_V4l2FdCommon`.

`0x0092e8ac(fd, type)`, also unnamed: if `type <= 25`, r6 =
table A `[type]` at `0x013b9e78` and r2 = table B `[type]` at
`0x013b9ee0`; otherwise both 0. Then
`Hal_Video_Vpltio_SendCmd(0x4c4, 0, &{tableA, tableB}, five zero stack
words)` and `Hal_Vin_SelectInput(fd, tableA)`. Both returns are
ignored. The helper returns 0.

Call-site argument placement, from the stores before the calls:

- Open(0): `Set_V4l2Info(slot, slot, fd, word2 preserved from Get, type)`.
  The new fd is written over the first word of the FdCommon buffer, so
  it is r0 of `Set_V4l2FdCommon`. Five further stack words are copied
  from the Get buffer.
- Open(1), type fixed to 10: same shape; the fd is written so it is r1
  of `Set_V4l2FdCommon`.
- Open(4), type 19: fd is written so it is r2 of `Set_V4l2FdCommon`.
- Dual path, types 14..18: `Open(3)`; if `fd < 0`, fail.
  `0x0092e8ac(fd, type)` and no `SetStreamStart` on this fd. That fd is
  stored as `Set_V4l2Info` r3 and as the first stack word of
  `Set_V4l2FdCommon`. Then `Open(2)`; if `fd < 0`, fail.
  `Hal_Vin_SelectInput(fd2, 0)` — r1 is the immediate 0, not table A.
  `SetStreamStart(fd2)`. `Set_V4l2Info(slot, slot, fd2, first fd, type)`.
  fd2 is stored so it is r3 of `Set_V4l2FdCommon`.

`Hal_Vin_Open` / `SelectInput` / `SetStreamStart` / `Set_V4l2Info` /
`Set_V4l2FdCommon` / `Vpltio_SendCmd` bodies are in libNDAL3 and were
not read. The scalar arguments above are only the aurora call sites.

### Input-id tables (ELF, signed int)

Table A `0x013b9e78`, 26 entries, matches `g_input_id` in
`openaurora/modules/source/source.c`:

`1,0,0,0,0,0,0,0,0,0, 2,0,0,0,0,1,2,3,3,66, 3,3,0,0,0,255`

Table B `0x013b9ee0` is 0 except index 5 and index 21, which are 1.
`slot_connect` accepts only the jump-table types. Type 25's id 255 is
not used by `slot_connect` (type `> 21` returns `-1`). Types 1, 2 and 3
also return `-1` from `slot_connect`; `ChangeInput` handles 1, 2 and 3
on the path above instead. Types 14..18 use table A only on the first
fd (`0x0092e8ac`); the second fd's `SelectInput` argument is 0. Type 19
uses table id 66 with `Open(4)`. The UI index-to-device map inside
`ICE_ExternalGetVideoDevice` was not decoded, so TV versus HDMI versus
VGA is stated as these type numbers, not as menu labels.

### What `source.c` does not do

`select_type` (`openaurora/modules/source/source.c`) calls
`Hal_Vin_SelectInput(fd, g_input_id[type])` and
`Hal_Vin_SetStreamStart` on one fd from `Hal_Vin_Open(0)`, and only
when `source.enable` is set. `source.enable` is module policy. The
firmware sequence that is absent:

1. `PLAT_VIU_ChangeInput`'s type gates, including the type-1
   `PLAT_DEC_SdVideoConnect` path and the type-2/3 skip of
   `slot_connect`.
2. `plat_vin_slot_connect`'s per-type `Hal_Vin_Open` index
   (0, 1, 3 then 2, or 4), not a single `Open(0)`.
3. `Hal_Audio_EasySelect(0, 1, &{1, "adc on"})` for types 4..21 before
   the connect, return ignored.
4. Already-connected short-circuit on slot word `+4`.
5. VPLT command `0x4c4` with the `{tableA, tableB}` pair, then
   `SelectInput(fd, tableA)`. On the dual path, a second
   `SelectInput(fd2, 0)` and `SetStreamStart` only on fd2.
6. `Get_V4l2Info` / `Set_V4l2Info` and `Get_V4l2FdCommon` /
   `Set_V4l2FdCommon` with the fd spliced into a different word per
   open index.
7. `plat_drm_connect_dpsource`, `NDAL_VideoExt_OpenDevice`, HDMI phy
   get/set for types 14..18, `PLAT_VIU_NdVideoSetting`,
   `PLAT_SRCIN_GetVinParam`, `PLAT_VIN_UpdateTDLayerInputSource`.
8. The `AudioIF` stop / `setTransportStreamSource` /
   `startNonDVBAudio` sequence in `selectExtSource`.
9. Failure return `-1` for types the jump table rejects, instead of
   applying table A anyway. Types above 21 must fail. A failed connect
   still writes the slot record when `slot <= 6`.

## Mute / audio

### Key to hardware

`SettingsDialog::onMuteKey` at `0x00d56d00` loads `*(this+0x484)` and
calls `DemoApp::toggleAndDisplayMuteState` (`0x00cd82a8`). It always
returns 1. The original key message is not passed. Whether the
settings message map calls `toggleAndDisplayMuteState` directly was
not re-read; the named handler does call it.

`toggleAndDisplayMuteState`: when byte `DemoApp+0x1278` is 0, r1 is
that 0 (`ldrb` / `cmp` / `bne` at `0x00cd8378`) and is not rewritten
before `0x00cd8382`: object `*(DemoApp+0x31c)`, vtable `+0x24`.
`createApplicationModules` stores `NovoSystemInitialiser::soundSettings()`
there; `soundSettings` (`0x00f21f9c`) returns `*(this+0xb8)`.
`NovoSoundSettings` vtable `0x0150e008`, slot `+0x24`, is
`toggleMuteStatus(bool)` at `0x00f445dc`. So this call is
`toggleMuteStatus(false)` in the sense that r1 is 0. The callee does
not store that 0 unchanged (below). The `VolumeDialog` construction
path (`0x00cd82f4`) reads vtable `+0x28` and `+0x144` and does not
itself call `toggleMuteStatus`.

`toggleMuteStatus`: if the object at `this+8` has a nonzero vtable
`+0x5c` result, it skips every mute call and goes to the epilogue.
That object is not named. Otherwise it calls
`EASITVControlInterface::isMHEGClientMuted(this+28)` and its own
vtable `+0x28` `isAudioMuted`. If both are 0 it calls
`EASIControlInterface::muteOn(this+20)`; if either is nonzero it calls
`muteOff`. It then rewrites the byte at `this+0x30` (forced to 1 when
muting and `getCommonVolumeControlAvailability` at vtable `+0x190`
returns 0; on unmute the stored byte follows the availability result
or the MHEG flag). Headphone virtuals `+0x148` / `+0x14c` run on some
of those branches. After that it always calls
`NovoCECController::reportSystemAudioStatus(handle, UserPreferences
volume, mute byte)`. That is a CEC status report, not an SPDIF
control. `enableSpdifOut` (vtable `+0x0fc`, `0x00f43b68`) and
`setDigitalOut` (`+0x0c8`, `0x00f46044`) are not called from
`toggleMuteStatus`.

`EASIControlInterface::muteOn` (`0x00975d58`) / `muteOff`
(`0x00975d8c`) forward to `EASIControlTask::muteOn` (`0x0098bb00`) /
`muteOff` (`0x0098bb44`). Those call object `task+0xc4` vtable `+8` /
`+0xc` with r1 = the word at `task+0x384`, and store 1 or 0 at
`task+0x34a`. The virtual's return is ignored. The `+0xc4` object's
assignment was not read. The slots match `AudioOutputControl`
(`muteOn` `0x00a6141c`, `muteOff` `0x00a61498`). Its constructor
(`0x00a612dc`) clears four reason bytes at `this+32..35`.

`AudioOutputControl::muteOn(int reason)`: if `reason > 3`, return
without a HAL call. Otherwise set `this+32+reason = 1`. If
`isMutedInternal(false)` is 0, return without a HAL call. Otherwise
`IceAudioIF::setVolume(0, 1)` through the object at `this+4`, vtable
`+0xc`. `muteOff` clears the reason byte when `reason <= 3` and, when
no reason remains and a saved level exists, calls
`setVolume(saved scaled level, 0)`. Mute-on is therefore
`setVolume(0, true)`, not a separate mute opcode.
`IceAudioIF::setVolume` (`0x00a70bf4`) locks and calls
`ICE_AudioVolume(this+8, volume, flag)`. The return is ignored.

The reason word at `EASIControlTask+0x384` was not recovered. If it is
greater than 3, `muteOn` performs no audio call.

### `ICE_AudioVolume` (`0x007df2ec`)

On the main path (`word [context] == 0`; a nonzero context word takes
the AD branch at `0x007df4a2`, which can call `PLAT_AOUT_SetADVolume`
and then returns without the three calls below):

- Clamp the volume argument (r8) to 100.
- If the mute flag r5 is 0, and a cached word at `[r4]` is 1, and
  `DEV_GPIO_EP_IsPlugIn` is nonzero, and
  `_ice_Sound_Control_GetSoundOutput` returns 0 (`0x007df3e0`):
  `PLAT_AOUT_SetSpeakerMute(*(uint32*)literal, 0)`, `FROST_Sleep(500)`,
  `DEV_AMP_GetAmp(0)`, `DEV_AMP_GetMuteStatus`, and if the status word
  is 1, `DEV_AMP_Mute(amp, 0)`. That block is an unmute. Mute-on
  (`r5 != 0`) branches over it to `0x007df45c`.
- At `0x007df45c`, if r5 differs from the cached word, 
  `PLAT_VAUD_SetSoftMute(uxtb(r5), 0)` and store r5 at the cache and
  at cache `+16`.
- Always `PLAT_SAMBA_SetTVVolume(r8)` and `PLAT_SAMBA_SetTvMute(r5)`.

`PLAT_SAMBA_SetTvMute` (`0x00921334`) only executes
`strb r0, [0x017726f0+26]`. `PLAT_SAMBA_SetTVVolume` (`0x009216a4`)
only executes `strb r0, [0x017726f0+27]`. Neither calls audio, SPDIF,
the amp, or `Hal_Audio_Ctrl`. They return with r0 still the input
byte. Those two Samba calls do not mute the optical output.

`PLAT_VAUD_SetSoftMute(flag, 0)` (`0x008d1eb8`): if the second argument
is 0 and it equals the byte at `0x0197f9bc`, return 1 without the
worker. Otherwise lock mutex `0x0197f9c0` (lock failure returns 0) and
call unnamed `0x008d11f0(flag, 0)`. Because the second argument is 0,
`0x008d11f0` does not take the extra-speaker block. It calls
`PLAT_AOUT_SetSpeakerMute(*(uint32*)0x0176da40, byte(0x0197f96d) | flag)`.
If `(flag & byte(0x0197f99c)) != 0` it also calls
`PLAT_AOUT_SetMute(0, 0)`, which is a clear of mute id 0, not a mute-on.
`0x0197f99c` and `0x0197f96d` are BSS; their runtime values were not
recovered. The image word at `0x0176da40` is 0.

`PLAT_AOUT_SetSystemMute` (`0x008d1f74`) calls `0x008d11f0(byte, 1)`
and is not on this path. With second argument 1 that worker also mutes
the ids at `0x0176da00`, `0x0176da6c`, and the immediate id 3. The
remote key passes 0, so those three are not called.

### Speaker id and SPDIF

`PLAT_VAUD_SetSpeakerMap(a, b, c)` (`0x008d1b7c`) stores a, b, c at
`0x0176da40`, `0x0176da00`, `0x0176da6c`. Its only caller is
`ICE_AudioInit` at `0x007e16aa`, which loads three words from
`0x0175d8dc`. That image is `{0x7fff, 1, 2, 1}`. The call overwrites
the first word before the load:

- `Profile_GetMainSpeakerPower` equals `6Watt` or `6Watt_12V`, or any
  other string including the `8Watt` print path: store 0, then
  `SetSpeakerMap(0, 1, 2)`.
- Equals `2.5Watt`: `SetSpeakerMap(1, 1, 2)`.

The profile key is `AudioSettings/Prescales` / `DefaultSpeaker`
(lead: `artifacts/analysis/audio-amp-profile-2026-11-04/metadata.txt`).
Which string this set stores was not read. The soft-mute speaker id is
therefore 0, or 1 if the power string is `2.5Watt`.

`PLAT_AOUT_SetSpeakerMute(id, mute)` (`0x00888c94`): if `id > 8`,
return 0. Record is `0x0197a180 + id*0x3c` (60 bytes). Byte `+16` is
the present flag. If it is 0, printf and return 0 with no HAL call.
Word `+12` must be `<= 2` or the function returns 0. It stores the
mute byte at record `+17`, calls gate `0x00886d4c`, and only if that
returns nonzero calls worker `0x00887040(record)` then `0x00886df4`.
The worker's return becomes the function return. `0x00886d4c` was not
disassembled; a zero return skips the HAL write.

`PLAT_AOUT_Init` (`0x0088744c`) `memset`s each 60-byte record and then
writes id `+8` = the index 0..8, output id `+12` = `0x7fff`, present
`+16` = 0, mute byte `+17` = 1. The worker `0x00887040` switches on
word `+8`, which init sets to the speaker index:

| `+8` | call | trace string in the same block |
|---|---|---|
| 0 | `Hal_Audio_KItem(0x43)` then `Hal_Audio_SetControl` on `{0x43, &mute byte at +17, 0}` | `NDAL_Sound_SpeakerMute(ND_SPEAKER_MAIN, %d)` at `0x0136d3c0` (the pointer lands mid-string, on `kerMute(...)`; the full rodata entry names `ND_SPEAKER_MAIN`) |
| 1 | `Hal_Audio_EasySet(0x46, &mute byte, 0)` | `NDAL_ExtOut_AudioMute(ND_EXTOUT_AUD_4, %d)` |
| 2 | `Hal_Audio_EasySet(0x4e, &mute byte, 0)` | `NDAL_ExtOut_AudioMute(ND_EXTOUT_AUD_1, %d)` |
| 3 | `Hal_Audio_EasySet(0x4b, &mute byte, 0)` | `NDAL_ExtOut_AudioMute(ND_EXTOUT_AUD_SPDIF, %d)` |
| other | return 1, no HAL call | |

`SetControl` / `EasySet` returns are ignored; the worker returns 1.
SPDIF is case 3. The remote path's id is 0 or 1, so the instructions
reach `ND_SPEAKER_MAIN` (opcode `0x43`) or `ND_EXTOUT_AUD_4` (opcode
`0x46`). They do not reach case 3. No `ICE_SPDIFEnable`,
`ICE_SoundControlEnableSPDIFOut`, `ice_SPDIFGPIOEnable`, or
`PLAT_AOUT_SetSPDIFTranscodeType` call is in `ICE_AudioVolume` or in
`0x008d11f0`. Whether `Hal_Audio_KItem(0x43)` inside libNDAL3 also
gates the optical transmitter was not read.

`PLAT_AOUT_SpeakerConnect` (`0x00888498`) does not set present. If
present is already 0 it only prints. The store that changes present
from the init value 0 to nonzero was not found. Until that store has
run, `SetSpeakerMute` does not call the worker. A 2026-11-01 live
sample (lead, `AUDIO-ROUTING-2026-11-01.md`) reported speakers 0 and 1
present; that sample was not re-read.

`PLAT_AOUT_SetSpeakerHWMute` (`0x00888fe0`) is a different function.
It stores the mute at record `+18` (not `+17`) and, after the same
present / output-unit / `0x00886d4c` gates, calls `Hal_Audio_Ctrl`
with opcode `0x35`. The HAL return is discarded. The function return
is the present byte, or 0 on a bad id. The remote mute path does not
call it. `PLAT_AOUT_SetMute` (`0x00888950`) uses opcode `0x52` or
`0x5b` through `Hal_Audio_KItem` / `Hal_Audio_SetControl` and is only
the conditional `SetMute(0, 0)` above.

### Volume is a different argument

`TabDialog::onVolumePress` (`0x00d6edb4`) loads `this+0x4c0`, calls
vtable `+0x18`, and if that is nonzero calls vtable `+0x20` with the
key message. It returns 1. It does not call `ICE_AudioVolume`. The
class at `+0x4c0` is not recovered. The main-TV handler for ICE
`0x220` / `0x221` was not found.

Inside `ICE_AudioVolume` the volume and the mute flag are separate.
Mute-on from `AudioOutputControl` passes volume 0 and flag 1, so both
`PLAT_SAMBA_SetTVVolume(0)` and `PLAT_SAMBA_SetTvMute(1)` run, and
`SetSoftMute(1, 0)` runs when the cached flag differs. A call with a
nonzero volume and flag 0 still calls `SetTVVolume(level)` and
`SetTvMute(0)`, and calls `SetSoftMute` only when the cached flag
changes. `NovoSoundSettings::setVolumeLevel` (`0x00f43f5c`) was not
disassembled, so the volume keys are not proved to be that call.

### What `audio.c` calls instead

`apply_mute` in `openaurora/modules/audio/audio.c` calls
`Hal_Audio_Ctrl(0x35)` with `params[0] = 0` and `params[1] = on ? 1 : 0`,
and returns the HAL status. Opcode `0x35` is the inside of
`PLAT_AOUT_SetSpeakerHWMute`, which the mute key does not call, and
which discards the HAL return. The key's HAL operation is
`Hal_Audio_KItem(0x43)` plus `Hal_Audio_SetControl` for speaker id 0
(`ND_SPEAKER_MAIN`), or `Hal_Audio_EasySet(0x46, ...)` if the speaker
map id is 1. Both go through `PLAT_AOUT_SetSpeakerMute` after the
present, output-unit, and `0x00886d4c` gates, and only from
`PLAT_VAUD_SetSoftMute`. Passing logical id 0 to opcode `0x35` is a
different interface. Why the TV's `Hal_Audio_Ctrl(0x35)` returns `-1`
is in libNDAL3 and was not recovered. The stock wrapper would not
surface that `-1` even on the HW-mute path.

## Key actions

Handlers verified from instructions. Message-map immediates in
`SETTINGS-INPUT-2026-11-03.md` were not re-dumped.

`SettingsDialog::onMenuPress` (`0x00d56d38`) reads
`UIKeyPressMessage::key()`:

- `0x210`: r1 = `0x29a`, then vtable `+0x78`.
- `0x22d`, `0x25b`, or `0x251`: `DemoApp::resetPictureSettingsLastSelectedIndex(*(this+0x484))`, then r1 = 0, same virtual.
- any other key that reaches the function: r1 = 0, same virtual.

Always returns 1. `KeyExit` (`0x210`) and `KeyExit2` (`0x25b`) differ.
`0x22d` (`KeyMenu`) takes the same path as `0x25b`, and so does
`0x251`.

SettingsDialog vtable `0x014dbea4`, object slot `+0x78` (file
`0x014dbf24`), holds `0x006b14ac`, which is `__cxa_pure_virtual@plt`.
Slot `+0x84` is the same PLT. TabDialog vtable `0x014ddb7c` slot
`+0x78` is `TabDialog::cleanupAndEndDialog` (`0x00d6f974`), which
forwards to `SettingsDialog::cleanupAndEndDialog(int)` (`0x00d56dbc`)
with r1 preserved. That function:

- `DemoApp::updateTabDialogTitles(*(this+0x484))`.
- `SVGDialog::endDialog(this, code)`. The return is ignored.
- If code is 0: `setLastSelectedLauncherItem(demo, 0)` and
  `setLastSelectedMainLauncherIndex(demo, -1)`. Code `0x29a` does not
  clear those.
- `DemoApp::checkMuteState`, then `shouldReturnToPortal`.
- If the portal call returns 0, or the code is `0x29a`, return.
  Otherwise allocate 16 bytes, construct `UIKeyPressMessage(0x239)`,
  and `UIFramework::postMessage(this+4, msg, 1)`.

So `0x210` ends the dialog with `0x29a`, does not clear the launcher
indices, and does not post `0x239`. `0x25b` and `0x22d` reset the
picture index, end with 0, clear the launcher indices, and post
`0x239` only when `shouldReturnToPortal` is nonzero.
`TabDialog::onMenuPress` (`0x00d6ee34`) calls
`SettingsDialog::onMenuPress` and forces return 1.

`SettingsDialog::onUpDownPress` (`0x00d56b5c`): if
`(*(this+0x490) - *(this+0x48c)) >> 2` is nonzero,
`navigateInList(this, message)`; otherwise
`UIMessageTarget::handleKeyPressMessage(this+0x17c, message)`. Then
`UIFramework::updateDisplay(*(this+4))`. Returns 1. It does not
compare `0x20a` with `0x20b`. `navigateInList` was not re-read.

`SettingsDialog::onSelectPress` (`0x00d56bb0`):
`handleKeyPressMessage(this+0x17c, message)`, then vtable `+0x84`.
Returns 1. On a TabDialog that slot is `0x00d6eb08`. That body was
not read. On a SettingsDialog the slot is `__cxa_pure_virtual@plt`.

`TabDialog::onVolumePress` is described in the mute section. ICE
`0x222` reaches `toggleAndDisplayMuteState` through `onMuteKey`. The
main-TV handlers for `0x220` / `0x221` were not found. Source-dialog
key handlers were not separately verified.

## Not recovered

- libNDAL3 bodies: `Hal_Vin_Open` device node, `Hal_Vin_SelectInput`,
  `Hal_Vin_SetStreamStart`, `Hal_Video_Set_V4l2Info`,
  `Hal_Video_Get_V4l2Info`, `Hal_Video_Set_V4l2FdCommon`,
  `Hal_Video_Get_V4l2FdCommon`, `Hal_Video_Vpltio_SendCmd` for command
  `0x4c4`, `Hal_Audio_EasySelect`, `Hal_Audio_EasySet`,
  `Hal_Audio_KItem`, `Hal_Audio_SetControl`, and why
  `Hal_Audio_Ctrl(0x35)` returns `-1`.
- The class at `ExternalAVSourceManager+0x24` and whether vtable
  `+0x34` is `ICE_VideoSelectSource`. The class at `TabDialog+0x4c0`.
  The class at `NovoSoundSettings+8` whose vtable `+0x5c` can skip mute.
- `ICE_ExternalGetVideoDevice`'s index-to-device table, so menu labels
  (TV, AV, HDMI, VGA) are not tied to type numbers here.
- The writer of `PLAT_VIU_ChangeInput`'s stack word at `[r7,#4]`, and
  the branch at `0x00902ec2`.
- The call arguments at `0x008869da` and `0x008f49d4`.
- The value of `DefaultSpeaker` in this set's hwprofile. It selects
  speaker id 0 versus 1.
- The instruction that sets speaker-record present (`+16`) from 0 to
  nonzero, the runtime bytes at `0x0197f96d` and `0x0197f99c`, and the
  reason word at `EASIControlTask+0x384`.
- Gate `0x00886d4c`. If it returns 0, `SetSpeakerMute` does not call
  the HAL worker.
- Whether `Hal_Audio_KItem(0x43)` / `Hal_Audio_SetControl` inside
  libNDAL3 also stops the optical transmitter. The aurora path does
  not call an SPDIF symbol and does not select speaker case 3.
- `NovoSoundSettings::setVolumeLevel` and the main-TV handler for
  `0x220` / `0x221`.
- `TabDialog` vtable `+0x84` at `0x00d6eb08`, `navigateInList`, and the
  settings/tab message-map bytes. Source-dialog key dispatch.
- `ICE_AudioVolume`'s AD branch (`context` word nonzero) beyond the
  `PLAT_AOUT_SetADVolume` call and the skip of `SetTvMute`.
