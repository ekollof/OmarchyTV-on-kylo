# High-level AudioIF source transport — recovery note, 2026-11-09

Status: **firmware facts recovered; native implementation not yet possible
without porting the aurora PLAT_AOUT/AIN/VAUD route. No TV access.**

## Call chain

`ExternalAVSourceManager::selectExtSource` `0x00a2668c` (Grok report
`firmware-grok-2026-11-08`) calls, after a nonzero video-virtual return:

- original bool nonzero: `AudioIF::stopAudio(1)` (return ignored),
  `ICE_ExternalGetAudioDevice(index)`,
  `AudioIF::setTransportStreamSource(device)`; if that returns nonzero,
  `AudioIF::startNonDVBAudio()`;
- original bool zero: `AudioIF::stopAudioDescription()`; if the second audio
  object is present, `stopAudio(1)`/`stopAudioDescription`; then
  `setADTransportStreamSource(device)`; a zero return skips the starts;
  otherwise `startNonDVBADAudio()` and, if present, `setTransportStreamSource`,
  `setADTransportStreamSource`, `startNonDVBAudio`, `startNonDVBADAudio`.

The `AudioIF` object layout from its constructor `0x00a60cbc`:
`+0x28 AnyIceAudioIF`, `+0x2c AnyCallbackMonitor`, `+0x30 AudioArbitrator`,
`+0x34 AnyDvbAudioPlayer`, `+0x38 AnyMp2AudioPlayer`,
`+0x3c AnyAudioOutputControl`, `+0x44`/`+0x48` null-instance interfaces,
`+0x70` a gate, `+0x74 FrostMutex`.

## What the methods actually do

- `AudioIF::setTransportStreamSource(device)` `0x00a5f6e8` asks
  `AnyDvbAudioPlayer` (`+0x34`, vtable `+0x10`) and, when `+0x70 == 0` and the
  result is nonzero, calls the second DVB player (`+0x48`, vtable `+0x10`) with
  the device.
- `AudioIF::startNonDVBAudio()` `0x00a5f888` calls the first DVB player
  (`+0x34`, vtable `+0x20`). `startNonDVBADAudio()` `0x00a5f8dc` calls the
  second (`+0x48`, vtable `+0x20`).
- `AudioIF::stopAudio(bool)` `0x00a5fa0c` calls the IceAudioIF (`+0x28`,
  vtable `+0x60`/`+0xc`), the second DVB player (`+0x48`, `+0x24`), the first
  DVB player (`+0x34`, `+0x24`) and `AudioArbitrator::deactivateActivePlayer`.
- `DvbAudioPlayer::setTransportStreamSource(device)` `0x00a6d06c` calls
  `IceAudioIF::setDeviceSource(device)` only when the player is not decoding
  (`this+0x38 == 0`); if decoding it only compares the requested device with
  `IceAudioIF`'s current device.
- `IceAudioIF::setDeviceSource` `0x00a70c50` calls
  `ICE_AudioSelectSource(this+8, device)` unless the source is unchanged.

## Why it cannot be called as a library

`ICE_AudioSelectSource` `0x007e2e14` is in aurora, not a shared library. Its
external-source route calls, among others:

```text
PLAT_AOUT_SetSystemMute()
PLAT_AOUT_SetMute(0,1)
PLAT_AOUT_Disconnect(0)
PLAT_AIN_Disable(...)
PLAT_AIN_Disconnect(...)
_ice_AudioSwitchPath2MM(0)
_ice_AudioSetDigitalOut(...)
_ice_Set_AMP_UNMUTE()
PLAT_VAUD_SetDecGainBase(0,0,0)
PLAT_AOUT_SetSpeakerDelay(0/1/3, ...)
```

These are aurora-internal PLAT wrappers over the audio HAL/amp/DSP, not
exported libNDAL3/libNDAL_EX entry points. There is no `AudioIF`/`AudioArbiter`
object to call in our process, and constructing the vendor classes would pull
in the whole C++ audio stack.

`ICE_ExternalGetAudioDevice` `0x007f31b0` reads a 13-record table at
`0x0175de8c`, stride `0x38`, field `+4`. The first records read
`0x22, 0x22, 0x23, 0x29, 0x25, 0x26`; the later record interpretation was not
fully resolved, so these are not yet a complete menu-label mapping.

## Next fact/implementation step

Port the PLAT_AOUT/AIN/VAUD route (and the relevant AudioArbiter/DvbAudioPlayer
state) to the existing `mod_audio`/`audio_mixer`/`audio_amp` services, then add
an `audio.source` service that `mod_source` can call. Do not fake the hardware
route or treat a successful `Hal_Audio_Ctrl` as audible acceptance. The source
gate stays 0 until that service and TV acceptance exist.
