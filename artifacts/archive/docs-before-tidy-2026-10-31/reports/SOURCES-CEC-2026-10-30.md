# How aurora handles sources (inputs) and CEC — 2026-10-30

Offline RE from `aurora.elf` (symbols + Ghidra) and the tables in `.rodata`. Everything here is from the
shipped binary; no TV needed.

## The layers

```
UI          NovoExternalAVSourceNavigator   (the "Bronlijst" list: count/name/type/index per window)
              │  getExternalInterfaceName / getFirstExternalInterfaceIndex / …
policy      InputManager                    (what is displayed, timeouts, per-window state)
              │
engine      EASITVControlInterface  →  ExternalAVSourceManager  →  ICE_External*   (statically linked)
              │                                                        │
platform    PLAT_*  (PLAT_VIN_*, PLAT_SRCIN_GetSourceName, PLAT_TVOUT_SetSource, PLAT_CEC_*)
              │
hardware    Novatek HDMI RX / MCU  (_HW_MCU_SetCECEnableOnOff, _HW_MCU_SetCECAutoPwrOnOff)
```

The *list itself* is owned by the ICE layer, not by the UI: `ExternalAVSourceManager::getNumberOfExtInterfaces()`
is literally `ICE_ExternalNumberOfInterfaces()`, and `getNameOfExtInterface(i)` is `ICE_ExternalGetName(i)`.

## The interface table

`ICE_ExternalGetName(i)` indexes a descriptor array (`base + i*0x38 + 0x2c` = name pointer, **max 13
interfaces**, each **0x38 bytes**). The per-entry getters that go with it:

```
getTypeOfExtInterface / getVideoType / getAudioType / getMode
getVideoGroupIDOfExtInterface / getVideoConnectorTypeOfExtInterface / getAudioConnectorTypeOfExtInterface
ICE_ExternalGetCapability / GetAspectRatio / IsHDMIAudioReturnChannelSupported
```

Names are in three clusters in `.rodata` (order in the descriptor array):

```
TV · BACK AV · HDMI1 · HDMI2 · HDMI3 · HDMI4 · S-VIDEO · SIDE AV · EXT1-S · YPbPr · VGA/PC · Wireless Display
```

which matches the Bronlijst we see on screen (TV, BACK AV, HDMI1-3, YPbPr, VGA/PC, Wireless Display) —
the set simply has the others disabled (`SRC_CFG_SourceSupportTable`, see below).

## Switching a source

`ExternalAVSourceManager::selectExtSource(index, secondary)`:

1. current/previous indices are per window (`this + (secondary?0:1)*4 + 0x34`), so main and PIP have
   separate sources;
2. `ICE_ExternalGetVideoType(index)` selects the path (types 1/2/5/6 = composite / scart / HDMI / VGA
   families, 4-ish = none);
3. video: `ICE_ExternalGetVideoDevice(index)` → the video device is bound through the vtable
   (`Open` / `SetVideoOutput`), then `setVideoDevice(...)`;
4. audio is routed **separately**: `AudioIF::stopAudio` / `setTransportStreamSource` /
   `startNonDVBAudio`, with an independent **audio-description** path
   (`setADTransportStreamSource` / `startNonDVBADAudio`) — so AD follows the interface, not the decoder;
5. `publishExternalSourceState(index, status)` tells the rest of the system (and the UI) about the new
   state; `externalVideoReadyEvent/LostEvent/AttributeChanged/AutoAdjustCompleted` carry the video side
   (signal found, resolution change, auto-adjust done);
6. ARC/HDMI audio return is a separate switch: `setHDMIAudioReturnChannel(bool, group)` +
   `isHDMIAudioReturnChannelSupported(group)`; SCART output has `muteScartOut(bool)`.

`InputManager` keeps the UI side: `interfaceName(i)`, `interfaceTypeName(i)`, `isExternalDisplayed`,
`isTunerDisplayed`, `setExternalDisplayed(win, bool)`, and the `timeout(i)` that makes the Bronlijst
disappear after ~2 s (the behaviour we had to work around when scripting keys).

## CEC

CEC in aurora is a **thin transport + policy**, with the protocol work in ICE:

| piece | role |
|---|---|
| `CECInterface` (FROST thread + 0x14-deep message queue) | `initialise()` → `ICE_CECInit` → `ICE_CECOpen` → thread; `main()` pumps `ICE_CECSendData`; `read(len, buf)` parses a frame into `CECMessage{initiator, destination, opcode, payload}` and `publishCECMessage()`s it |
| `CECMessagePublisher` | register/deregister listeners; fans messages out |
| `NovoCECController` | `getActiveLogicalDevice()`, `getDeviceType(logical)` / `setDeviceType(...)`, `isAudioStatusMuteOn()`, `getAudioStatusVolume()`, `isRCPassthroughSupported()`, `getCECEnabled()`, `registerCECEventsListener(...)` |
| `ExternalAVSourceManager` / `AVSourceManager` | `deviceConnected(u32)`, `deviceDisconnected(u32)`, `deviceStatusChange(u32)` — hotplug/CEC notifications that refresh the interface list and current index |
| MCU / profile | `_HW_MCU_SetCECEnableOnOff`, `_HW_MCU_SetCECAutoPwrOnOff`, `Profile_IsCECEnabled`, `Profile_IsCECAudioControlEnabled`, `Profile_GetCECString` (= the CEC device name), `ICE_LowPowerSetCECEnable`, `ICE_LowPowerSetCECOneTouchPlay` |
| debug/act flags | `ActFlag[... 6-CEC, 7-CEC one touch play]` printed at boot |

**Key mapping** (`CECKeyMapper::m_key_mapping`, 50 pairs, `.rodata 0x152f8a8`) translates *this* TV's ICE
key codes into **CEC user-control codes** — i.e. what we send when the remote is forwarded to another
device over CEC:

```
ICE 512..521 → CEC 0x20..0x29   (KeyZero..KeyNine → digits 0-9)
ICE 522..525 → CEC 0x01..0x04   (Up/Down/Left/Right)
ICE 526      → CEC 0x00         (Select)
ICE 528/603  → CEC 0x0D         (Exit)
ICE 527      → CEC 0x0B         (Back/Return)
…            → 0x0C (Mute), 0x30/0x31 (Volume up/down), 0x4X (menu/EPG/…)
```

So CEC **does not appear in the source list as its own entry** — a CEC device shows up as the HDMI port it
is plugged into, and what CEC really does here is: logical address/device type bookkeeping, audio status
(mute/volume) mirroring, RCP (remote-control passthrough) and one-touch-play / auto power on/off.

`ACTIVE_SOURCE` appears only as debug text (`_dbg_ice_cec_ActiveSource`, `CEC Active Source`, and a
`NovoTesttool::getSource` dump) — the active-source handling itself is inside ICE, not in the UI code.

## Which sources exist / are enabled

* `SRC_CFG_SourceSupportTable` (`.rodata 0x1770104`, 22 × `{u32 source, u32 enabled}`): enabled
  `0,1,2,3,4,10,14,15,16,19,20`; disabled `5..9,11,12,13,17,18,21`.
* `Profile_GetSourceSelection` + the settings DB blob `m_source_names` hold the user-facing list we saw
  earlier (`TV, BACK AV, EXT1-S, SIDE AV, S-VIDEO, HDMI1-4, YPbPr, VGA/PC, DVD, Wireless Display, APM`).
* `NEBULA_RegisterExternalSourceListUpdatedCallback` / `NovoExternalAVSourceNavigator::getNumberOfExternalInterfaces`
  is what the Bronlijst walks — so the list can change at runtime (hotplug / CEC).

## Practical notes for our launcher work

* the Bronlijst we open with `KeyAux` (1056) is this navigator's list; its ~2 s timeout comes from
  `InputManager::timeout(i)`, which is why scripted navigation must be a tight key burst;
* a "Sources" entry that *switches* (rather than opens the list) would call
  `ExternalAVSourceManager::selectExtSource(index, false)` — the same call the UI makes;
* anything CEC-related (one-touch-play, ARC, RCP) is policy in `NovoCECController` + `Profile_IsCEC*`, so it
  can be toggled from the settings DB without touching the engine.
