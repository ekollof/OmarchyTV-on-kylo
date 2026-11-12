# HDMI source-loss fallback — 2026-11-12

Status: current
Last verified: 2026-11-12
Evidence class: firmware instructions, REA cross-check, read-only on-TV
HPD/V4L2 captures, probe rejection logs and owner-reported stock behavior.
Corrects: the earlier 2026-11-12 conclusion that `0x9D Inactive Source` was
not the trigger. `SvgAppCECEventsListener::notifyCECDataMessageReceived`
handles opcode `0x9D` by constructing `UIKeyPressMessage(597)` and posting it
to the UI; `DemoApp`/`portal_closedstate` then selects DTV/tuner. The
`NovoCECController::handleCECMessage` `0x9D` return is a separate controller
path and does not cover this UI listener.
Corrected 2026-11-12: the private V4L2 event byte 4 is the
`VPL_SCLR_CTRL_UpdateSCLR` reason flag, not a source status; stock cable
plug/unplug uses HPD GPIO polling. The V4L2 event is not the source-loss
provider. Deck suspend keeps HPD high. CEC `0x9D` is the Deck-sleep trigger:
`SvgAppCECEventsListener` → `UIKeyPressMessage(597)` → DTV/tuner. The VIU
`LOST SIGNAL`/`ICE_VideoHide` lines occur as the TV leaves HDMI1 and are
consequences, not the trigger. The status-4 external-interface disconnect
policy remains the HPD cable-unplug fallback; the tuner service/view re-tune
remains open.

## Observation

The owner tested stock suspend/resume with the Steam Deck on HDMI1:

- Deck sleep: the TV stayed on and left HDMI1 for a tuner channel.
- Deck resume: the TV switched back to HDMI1.

The resume side is consistent with `0x82 Active Source` calling
`selectSource`. The sleep side is **CEC `0x9D Inactive Source`**:
`SvgAppCECEventsListener` posts `UIKeyPressMessage(597)`, and the UI selects
DTV/tuner. The stock log then shows `LOST SIGNAL`/`ICE_VideoHide` as the TV
leaves HDMI1; those are consequences. HPD stays high through suspend, and the
private V4L2 event is a scaler update, so neither is the trigger. The
external-interface disconnect policy below is the HPD cable-unplug fallback.

## Source identity

- `aurora.elf`, source
  `artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf`, MD5
  `64c252340d3af6c5d66072611b7bad7a`, SHA-256
  `7d6e4ca589f72135ae5e13c977a5bfae7f999abac7de701f8ac5c178f09a5993`.
- Canonical project `artifacts/workbench/ghidra_proj` / Aurora /
  `aurora.elf`, read-only `-noanalysis`.
- REA cross-check: `rea-agents@6.9.0`, Ghidra 12.2, function
  `0x00fb1fbc` on a hash copy.

## Verified fallback path

### 1. Interface-status callback enters the navigator

`ExternalInterfaceStatusCallback` @ `0x011684d4` is a dispatcher:

```text
ExternalInterfaceStatusCallback(param_1, param_2, listener)
  if listener != NULL:
      listener->vtable[0x30](listener, param_1, param_2)
```

It is the callback passed to
`NEBULA_RegisterExternalInterfaceStatusCallback` @ `0x0116f9b0`, which is called
from `initialise` @ `0x0116a060`. The exact adapter that registers the navigator
was not fully traced; the navigator's disconnect method is an EXTERNAL entry and
the dispatcher above is the reachable callback path.

### 2. Navigator to `InputManager`

`NovoExternalAVSourceNavigator::handleExternalInterfaceDisconnected` @
`0x00fb32f0` is a two-call thunk:

```text
0x00fb3304  ldr r0,[r0,#4]
0x00fb330a  bl  0x00fb1fbc      ; InputManager::handleExternalSourceDisconnected
```

`handleExternalInterfaceConnected` @ `0x00fb32bc` similarly calls
`InputManager::handleExternalSourceConnected` @ `0x00fb1f30`.

### 3. The fallback decision

This is the HPD/status-4 cable-unplug policy. The Deck-sleep path uses CEC
`0x9D`/key 597 instead; see section 7.

`InputManager::handleExternalSourceDisconnected` @ `0x00fb1fbc` takes the
disconnected external index. Its exact branch sequence:

```text
0xfb1fda  bl 0x00a23ea0   ; getVideoGroupIDOfExtInterface(disconnected)
0xfb1fde  ldr r1,[r4,#0x68]  ; current source index
0xfb1fe2  bl 0x00a23ea0   ; getVideoGroupIDOfExtInterface(current)
0xfb1fe8  ldrb.w r1,[r4,#0x88]
0xfb1fec  cbz r1,0xfb2004
0xfb1fee  cmp r5,r0
0xfb1ff0  bne 0xfb1ff8   ; different group: no fallback
0xfb1ff2  movs r1,#0
0xfb1ff4  strb.w r1,[r4,#0x88]
...
0xfb2004  cmp r5,r0
0xfb2006  bne 0xfb1ff8   ; different group: no fallback
```

When `this[0x88] == 0` and the disconnected source is in the current video
group, it:

- checks the service list and whether the tuner can be selected in the main
  window (`EASIServiceList::isEmpty` @ `0x0112a440`, `getServiceType` @
  `0x00fb0e40`, `canTunerBeSelectedInMain` @ `0x00fafd24`);
- turns teletext off (`0x009768d0`/`0x0097689c`);
- if `this+0x94 != -1`, restores the saved previous source index through
  `requestSelectExtSource(this, this+0x94, true)` @ `0x00fb1c50` and clears it:

```text
0xfb2048  bl 0x00fb1c50  ; requestSelectExtSource(this, this+0x94, true)
0xfb204c  str.w r5,[r4,#0x94]   ; this+0x94 = -1
```

- if `this+0x94 == -1`, checks interface `0x13` (`isInterfaceEnabled` @
  `0x00fafdf0`), and when the service list is non-empty selects the saved
  tuner position **`this+0x90`** through `requestSelectServiceForDisplay`:

```text
0xfb206a  ldr r0,[r4,#0x14]
0xfb206c  ldr.w r8,[r4,#0x90]
0xfb2070  bl 0x00f70540  ; TunerManager::getNovoServiceViewController
0xfb2074  ldr r2,[r0,#0]
0xfb2076  ldr.w r3,[r2,#0xc8]
0xfb207a  blx r3
0xfb208e  bl 0x00fb19f0  ; requestSelectServiceForDisplay(this, r8, true, ...)
```

The fields are saved when the external source connects:
`InputManager::handleExternalSourceConnected` @ `0x00fb1f30` writes
`this+0x94 = this+0x68` (previous external source index, `-1` when TV was
selected) at `0xfb1f68`, saves the tuner navigation position in `this+0x90` at
`0xfb1f76`, and sets `this[0x98] = 1` at `0xfb1fa0`. `this+0x68` is the current
external source index from `setCurrentExtSrc` @ `0x00fb11ac`; `-1` means no
external source (TV/tuner). The common cable-unplug-from-TV path therefore has
`+0x94 == -1` and uses the `+0x90` tuner-position path; `+0x94` restores a
previous external input when one was selected.

When `this[0x88] != 0` and the groups match, the code only clears `this[0x88]`;
it does not switch sources.

The saved tuner service is `this+0x90`; the previous external source is
`this+0x94`; the current source is `this+0x68`. `this+0x98` gates the
tuner/previous-source branch. Do not assign friendlier names without more
evidence.

### 4. Parallel notification path is not the fallback

`ExternalAVSourceManager::deviceDisconnected` @ `0x00a27094`/`0x00a270e4` posts a
status-4 message. `publishExternalSourceState` @ `0x00a26514` maps status 4 to
`ExternalAVSourceStatePublisher::externalAVSourceDisconnected` @ `0x00a56bc4`.
The `NovoController`/UI path maps the resulting event to
`InputManager::externalSourceStatusChanged` and hide-video/no-signal UI. In the
recovered code that path does **not** select the tuner. The tuner/previous-source
fallback is the navigator callback above.

### 5. Provider entry and status carrier

The stock callback is registered by `AitHub::initialise` @ `0x01169f80` through
`NEBULA_RegisterExternalInterfaceStatusCallback` @ `0x0116f9b0`; it passes
`ExternalInterfaceStatusCallback` @ `0x011684d4` (Thumb entry `0x011684d5`).
The registration stores an `ExternalInterfaceStatusChangeCallback` @
`0x0118e219` in the `NebulaExternalAVSourceNavigator` singleton.

The exported event entry is `NEBULA_HandleExternalInterfaceStatusChange` @
`0x0116fa38`. It has no internal caller in `aurora.elf`; it forwards to
`NebulaExternalAVSourceNavigator::handleExternalInterfaceStatusChange` @
`0x0118e73c`. Status values 1..6 dispatch through the
`AnyNovoExternalAVSourceNavigator` vtable slots `+0xb8..+0xcc`, which reach the
six `NovoExternalAVSourceNavigator::handleExternalInterface*` methods and then
`InputManager`. The exact enum labels are not recovered; the slot order follows
the method set above.

The kernel-side status block is `ndp_SRC_ConnectInfo` (0x74 bytes, runtime
`0x00bb0d98`) in `ntkdriver_nondovi.ko`:

- `NDP_ExtSrc_GetConnectInfo` @ `0x0061bb6c` returns its address;
- `NDP_ExtSrc_SetConnectInfo` @ `0x0061bb8c` copies 0x74 bytes into it;
- `NDP_ExtSrc_Initial` @ `0x0061d1d8` zeroes per-port flags at `0x0`, `0xc`,
  `0x14`, `0x1c`, `0x28`, `0x34`, `0x40`;
- `_VIU_CheckReconnect` @ `0x0061d868` reads those flags and calls
  `NDP_ExtSrc_ChangeInput` to reconnect the VIU.

`NDP_ExtSrc_g_ext_ctrls` @ `0x0061cd84` handles controls `0xa00966`,
`0xa00968`, `0xa0099f`, `0xa00967` and `0xa009ab`; it does not directly expose
the 0x74-byte block in the analyzed path. The `SetConnectInfo` caller and the
ICE/HDMI-switch event that invokes `NEBULA_HandleExternalInterfaceStatusChange`
are outside the analyzed driver path and remain unidentified. Do not guess a
V4L2 control ID or event from the address alone.

### 6. Private V4L2 event is a scaler update, not source loss

The private event on `/dev/video21` (`ndp-hdmi`) is a scaler update
notification:

- `_ndp_hdmi_core_vidioc_subscribe_event` @ `0x0000052c` accepts only event
  type `0xc000001`; it calls `VPL_SCLR_CTRL_Event_Subscribe` and
  `v4l2_event_subscribe`.
- `VPL_SCLR_CTRL_Event_RegisterCB` @ `0x00637538` stores `event_cb` at
  `0x00bb3e64 + 4`; `eventEnable` at `0x00bb3e64 + 0` is gated by bit 1.
  `VPL_SCLR_CTRL_Event_Subscribe` ORs bit 1; it does not replace the mask.
- `_CTRL_EVENT_Update` @ `0x006394bc` builds a 28-byte payload and calls
  `event_cb(0xc000001, &payload)`. Its caller
  `VPL_SCLR_CTRL_UpdateSCLR` @ `0x0063967c` passes `(source, arg == 6)`.
- Exact payload layout from the stores: `[0..3]` source type (little-endian),
  `[4]` `(UpdateSCLR arg == 6)` (`strb r6`), `[5]` `VidSetting+0x0c`,
  `[6..7]` `VidSetting+0x86` (little-endian uint16), `[8..0x1b]` scaler
  timing fields. The earlier `[4..5]`/`[6]`/`[7]` reading was wrong.
- `_ndp_hdmi_core_event_callback` @ `0x0000150c` copies those 28 bytes into
  the V4L2 event data and calls `v4l2_event_queue`.

Address convention: the `ntkndphdmi.ko` addresses are `.text` section offsets
(objdump/nm); the `ntkdriver_nondovi.ko` addresses are Ghidra runtime addresses
(`.text` offset + `0x10228` in this import).

A read-only capture ran while stock stayed up. `_ndp_hdmi_core_open` @
`0x00001820` is per-fd (`v4l2_fh_init/add`, callback registration) and release
@ `0x000016b8` only removes the per-fd fh; subscribe ORs the global enable bit.
The capture produced one event during the Deck sleep/resume window:

```text
05 00 00 00 00 00 70 17 00 0f 70 08 00 00 00 00 00 0f 70 08 00 00 00 00 00 0f 70 08
```

Source type 5, update flag 0, `VidSetting+0x0c` 0, `VidSetting+0x86` 0x1770,
and repeated 3840x2160 timing pairs. It is not a connected/disconnected or
video-lost status.

Pure decoder: `include/openaurora/source_hdmi_event.h` /
`common/source_hdmi_event.c`. `make check-source-hdmi-event` covers the
28-byte bound, source type, update flag, VidSetting fields, the exact live
sample, raw preservation and short/null rejection. The injectable adapter
`include/openaurora/source_hdmi_event_io.h` / `common/source_hdmi_event_io.c`
uses the target ABI (`sizeof(v4l2_event)=128`, data at offset 8,
`VIDIOC_SUBSCRIBE_EVENT=0x4020565a`, `VIDIOC_DQEVENT=0x80805659`) and
`make check-source-hdmi-event-io` covers open/subscribe, dequeue decode,
EAGAIN/wrong-type/error and cleanup. The adapter is not wired to source-loss.
Do not map its update flag to `ExternalSourceStatus` or the NEBULA status.

### 7. Stock HPD provider detects cable plug/unplug only

`ice_externalCheckCbStatus` @ `0x007f58ec` is the stock thread: it sleeps
1500 ms, then loops `_ice_CheckHdmiPlugStatus`, HDR polling, capture-protect
and PWM work, and sleeps 30 ms after the first iteration.

`_ice_CheckHdmiPlugStatus` @ `0x007f56c8` loops `uVar13 = 0..3` and calls
`PLAT_VIU_GET_HDMI_IDENT(uVar13)`, comparing against a per-port previous
byte. On a 0 transition it calls `_ice_hdmi_cb_func_to_app(pos,1)`,
`_ice_video_cb_func_to_app(7)` and `_ice_hdmi_cb_func_to_app(pos,2)`; on a 1
transition it calls `_ice_hdmi_cb_func_to_app(pos,0)`.

`PLAT_VIU_GET_HDMI_IDENT` @ `0x00901a38` computes `(arg - 1) & 0xff`; arguments
1..3 select table `0x0139f448` = 200, 201, 202, and argument 0 (or any other)
selects the `0xcb` fallback = 203. It calls `Hal_Gpio_Read(pin, &level)`.

`Hal_Gpio_Read` @ `0x4e7be4cc` has a separate pins-200..204 bulk branch: it
sends kernel getter `0x6712` and reads five bytes, with no sysfs export, pin
claim or GPIO configuration. The index table `0x4e7d614c` is
`[2,3,1,0,4]` for pins 200..204. `_ice_ext_GetHDMICallbackPosition(0..3)`
maps the same loop indices to the HDMI1..4 source-list positions 4..7
(`ExternalAVSourceManager` table `0x0175de8c`). Therefore:

```text
HDMI1 = arg0 = pin203 = bulk byte0
HDMI2 = arg1 = pin200 = bulk byte2
HDMI3 = arg2 = pin201 = bulk byte3
HDMI4 = arg3 = pin202 = bulk byte1
```

HDMI1=byte0 is live-confirmed; HDMI2..4 follow the same recovered loop/table
order. `extInterfaceCallback` @ `0x00a26e44` maps the ICE callback status through
table `0x013d847c` = `[3,4,2]`: 0 -> 3 connected, 1 -> 4 disconnected,
2 -> 2 attributes changed. `ExternalAVSourceManager::publishExternalSourceState`
@ `0x00a26514` confirms 3 = `externalAVSourceConnected`, 4 =
`externalAVSourceDisconnected`, 2 = `externalAVSourceAttributesChanged`.
`InputManager::externalSourceStatusChanged` @ `0x00fb02f8` consumes 3/4 for
HDMI interface type 7 and calls `NovoCECController::onHDMIDisconnected` on 4.

A read-only HPD probe (`oa-hpd-status`, GET `0x6712` on `/dev/ntgpio`, no HAL,
claim or export) read `01 01 00 01 00` while the Deck was on HDMI1: HDMI1
byte0 = 1. A 120 s capture through another Deck sleep/resume stayed exactly
`01 01 00 01 00`; HPD does **not** drop on suspend. HPD is cable plug/unplug,
not the sleep trigger.

The sleep trigger is **CEC `0x9D Inactive Source`**, not the video path.
`SvgAppCECEventsListener::notifyCECDataMessageReceived` @ `0x00d9766c` compares
the opcode and, at `0xd976a8`, matches `0x9d`; it then constructs
`UIKeyPressMessage(597)` (`movw r1,#597` at `0xd976b4`) and posts it to the UI
with push-front at `0xd977de`. The stock log matches this: key 597 reaches
`DemoApp::preHandleKeyPressMessage` and `portal_closedstate`, then
`ICE_CIDSetInputName` DTV, `ICE_VideoSelectSource` source 8 and
`TunerManager::selectServiceForDisplay(1)`. The key-597 emitter is therefore
recovered; the remaining contract is the DTV/tuner service/view selection.

The HDMI RX status is the **no-signal consequence**, not the trigger.
`plat_vin_slot_get_status` @ `0x0092ea68` calls `Hal_Vin_GetHdmiStatus`, which
issues GET ext-control `0xa00967` with a 0x14-byte output. The vendor getter
checks only the ioctl return; the target kernel leaves `error_idx == count`, so
the direct `oa-hdmi-vin` getter on `/dev/video21` reproduces it. The
sleep/resume capture shows the TV losing HDMI sync as it leaves the input:

```text
active      w0=1 w1=1 w2=1 w3=0 w4=0
sleep       w0=0 w1=1 w2=1 w3=0 w4=0xffffffff
            w0=5 w1=0 w2=0 w3=0 w4=0xffffffff
resume      w0=0 w1=1 w2=1 w3=0 w4=0xffffffff
active      w0=1 w1=1 w2=1 w3=0 w4=0
```

The stock log excerpt shows the ordering:

```text
Path0 Mode Change = LOST SIGNAL
viu callback event = 3
_ice_video_cb_func(): path=0, event=3
ICE_VideoHide
preHandleKeyPressMessage() key: 597
portal_closedstate: handleKeyPressMessage(): 597
ICE_VideoSelectSource(): Middle Ware input source id ---=8----
select source =8, path=0 (enSourceType=0)
PLAT_VIU_ChangeInput SourceID=0x0
TunerManager::selectServiceForDisplay(1)
```

The `LOST SIGNAL`/`ICE_VideoHide` lines are the HDMI input losing sync as the
TV starts the DTV switch; they are not the trigger. The resume side is
`CEC 0x82` → `requestSelectExtSource 4` → source 17 (HDMI1). The
`NovoCECController::handleCECMessage` `0x9D` return is a separate controller
path and does not cover the `SvgAppCECEventsListener` UI listener. The
video-lost methods only set flags and call CEC; they do not select the tuner.
`SignalLostDialog::onExternalSignalCheckTimer` runs a 1 s counter and
`endCurrentDialog`, but the switch here is immediate, so the 3-minute dialog
countdown is not the trigger.

The read-only VIN provider is implemented in the replacement:
`common/source_vin.c` / `include/openaurora/source_vin.h` decode `w0` and
expose an injectable poll seam; `mod_source` appends `video_status()` to
`oa_source_iface` v2, gated by `source.vin_status`. The method opens
`/dev/video21` lazily, returns 1 ready / 0 lost / -1 unavailable, and never
selects a source or tuner. `check-source-vin` covers the decoder and poll
seam; `tests/source_path.c` exercises ready/lost through the fake module; the
TV `mod_source.so` builds with `common/source_vin_vendor.c`. It is a no-signal
detector, not the sleep trigger.

The tuner service/view contract is partially recovered.
`TunerManager::requestServiceSelectForDisplay` @ `0x00f776b8` takes the
TunerManager mutex and calls `requestServiceSelectForDisplayInternal` @
`0x00f771e0`. The internal path rejects an empty `EASIServiceList`
(TunerManager+0x7c) and a pending standby request, then calls
`(TunerManager+0x1c)->vtable+0x54(position, view)` to resolve the service
handle and `+0xd4(handle, TunerManager+0x58, bool)` to select it, followed by
`NovoServiceNavigationInfo::notifyServiceSelected` and the mute/decoder/hide
sequence. The position→handle mapping over ACTIVECHANNELLIST and the stored
service list is implemented in `common/tuner_view.c` /
`include/openaurora/tuner_view.h`; `check-tuner-view` covers the runtime list,
stored order, identity order and rejection cases. The full select/mute/
decoder/UI contract remains open.

The source service now exposes `inactive_source(position)` v3 behind
`source.inactive_fallback`. It resolves the active `OA_SVC_CHANNELS` position
through `oa_tuner_handle_at`, connects DTV, and calls the `OA_SVC_TUNER`
`select(handle)` method. `tests/source_path.c` covers the gate, DTV/source
order, position→handle selection (100/200/300), missing-channel rejection and
the disabled gate. `mod_cec` does not yet route received `0x9D` to this method;
that is the remaining CEC module integration.

## What this establishes

- The stock **cable-unplug** fallback is an **external-interface disconnect**
  policy, gated by the disconnected source's video group relative to
  `this+0x68`. It is not the Deck-sleep path.
- The Deck-sleep fallback is **CEC `0x9D Inactive Source`**:
  `SvgAppCECEventsListener` constructs `UIKeyPressMessage(597)` at `0xd976b4`
  and posts it front. The UI selects DTV/tuner (`ICE_VideoSelectSource`
  source 8 → `TunerManager::selectServiceForDisplay(1)`). The key-597 emitter
  is recovered; the tuner service/view contract remains open.
- The VIU `LOST SIGNAL`/`ICE_VideoHide` lines follow the CEC-triggered source
  change; they are consequences, not the trigger.
- Stock cable plug/unplug uses HPD GPIO polling: HDMI1=pin203/bulk byte0,
  HDMI2=pin200/byte2, HDMI3=pin201/byte3, HDMI4=pin202/byte1; level 1 is
  connected and level 0 is disconnected. `oa-hpd-status` confirmed HDMI1=1
  with the Deck on TV. HPD stayed high through Deck suspend.
- The private V4L2 event `0xc000001` is a scaler update. Byte 4 is the
  `UpdateSCLR arg == 6` flag, not a source status; the live payload repeated
  3840x2160 timings. It is not the source-loss provider.
- The HDMI RX status `w0` is a no-signal detector: 1 while active, 0/5 after
  the TV leaves HDMI1. It is not the sleep trigger; the `source.video_status()`
  v2 provider exposes it read-only for no-signal UI/policy.
- `TunerManager::requestServiceSelectForDisplay` resolves a view position via
  `(TunerManager+0x1c)->vtable+0x54` and selects the handle via `+0xd4`. The
  position→handle mapping is implemented/tested in `common/tuner_view.c`
  (`check-tuner-view`); the full select/mute/decoder/UI contract is open.
- The source service's `inactive_source(position)` v3 worker is gated by
  `source.inactive_fallback`, resolves `OA_SVC_CHANNELS`, connects DTV and calls
  `OA_SVC_TUNER.select(handle)`. The fake-service `source_path` test covers the
  order/gates; `mod_cec` does not yet route 0x9D to it.
- On external connect, `this+0x94` saves the previous external source index
  (`-1` for TV) and `this+0x90` the tuner navigation position. On disconnect,
  `+0x94` is restored first; `+0x90` is the tuner-position path only when
  `+0x94 == -1`.
- The branch requires `this[0x88] == 0`, a matching video group, a non-empty
  service list for the tuner-position path, interface `0x13` enabled, and
  `this+0x98` set.
- `0x9D Inactive Source` **is** the Deck-sleep trigger through
  `SvgAppCECEventsListener`/`UIKeyPressMessage(597)`. The
  `NovoCECController::handleCECMessage` return path is separate and does not
  override the UI listener. `0x36 Standby` is not the trigger (the TV stayed
  on).
- REA's Ghidra provider recovered the same function and the same calls; its
  assembly at `0xfb2004`, `0xfb2048` and `0xfb208e` matches the canonical Ghidra
  dump exactly.

## Pure policy helper

`include/openaurora/source_loss.h` and `common/source_loss.c` transcribe the
recovered decision logic only. This is the HPD/status-4 cable-unplug policy;
the Deck-sleep path uses the CEC 0x9D/key-597 sequence above. The
helper:

- saves the previous external index and the tuner position on external connect
  and sets `auto_switched`;
- on a matching disconnect, a set `flag_88` is only cleared;
- otherwise restores the previous external index first, or returns the tuner
  position when `previous_external == -1` and `tuner_available`;
- performs no source select, tuner re-tune, HAL, device or UI I/O.

`make check-source-loss` covers TV→HDMI→tuner, external→external→tuner,
`flag_88`, `auto_switched`, group mismatch, invalid arguments and idempotent
connect. The full host suite passes. The helper is **not wired** into
`mod_source`: the CEC `0x9D` → DTV/tuner consumer, the
`requestSelectServiceForDisplay`/`TunerManager` service/view re-tune and the
5 s blue-background/UI timing are still open. Do not wire a guessed V4L2 event.

`include/openaurora/source_hpd.h` and `common/source_hpd.c` transcribe the
stock HPD mapping and event sequence only: pin 1..4 -> 203/200/201/202, bulk
index 0/2/3/1, first sample 1 -> connected, 1->0 -> disconnected then
attributes changed. `make check-source-hpd` covers the mapping, independent
ports, the exact `01 01 00 01 00` live sample, buffer/error gates and an
injectable bulk-getter adapter. `oa-hpd-status` is the read-only `0x6712`
probe. The HPD model is not wired into `mod_source` either: it detects cable
plug/unplug, not Deck suspend.

`common/source_vin.c` / `include/openaurora/source_vin.h` decode the five
`Hal_Vin_GetHdmiStatus` words and provide `oa_source_vin_poll` over an
injectable `oa_source_vin_io`. The production provider
(`common/source_vin_vendor.c`) opens `/dev/video21` read-only and issues GET
`0xa00967`; `mod_source` exposes it as `source.video_status()` (v2) only when
`source.vin_status=1`. `check-source-vin` covers ready/lost/null and the poll
seam; `tests/source_path.c` covers the service gate and live word sets through
the fake module. It is a no-signal detector and deliberately does not select a
source; the sleep trigger is CEC `0x9D`/key 597.

## Limits and next work

- The private V4L2 event `0xc000001` on `/dev/video21` is a scaler update; byte
  4 is the `UpdateSCLR arg == 6` flag, not a source status. The decoder and
  injectable adapter are tested, and a read-only live capture ran while stock
  stayed up; the adapter is not wired. Any `NDP_ExtSrc_SetConnectInfo` caller
  outside the analyzed RX/switch paths remains open. In `ntkndphdmi.ko`,
  `SetConnectInfo` is called from `tztv_hdmi_int_switch_connect_port` @
  `0x1070`, `_ndp_hdmi_core_release_func` @ `0x42b4` and
  `_ndp_hdmi_core_vidioc_s_input` @ `0x46e0`; `ntkndphdmiswitch.ko` calls it
  from `_ndp_hdmi_switch_vidioc_s_input` @ `0x7ac`. The 0x74-byte
  `ndp_SRC_ConnectInfo` per-port flags are at `0x0`, `0xc`, `0x14`, `0x1c`,
  `0x28`, `0x34`, `0x40`.
- The HPD provider detects cable plug/unplug only. `oa-hpd-status` confirmed
  HDMI1=1 with the Deck on TV, and a 120 s sleep/resume capture showed no HPD
  change. The HDMI RX status getter `Hal_Vin_GetHdmiStatus`/`0xa00967` works on
  `video21` when `error_idx` is treated as the vendor sentinel; it captured
  `w0 1 -> 0/5 -> 1` across suspend. `video20`/`video24`/`video28` reject. The
  sleep trigger is CEC `0x9D`: `SvgAppCECEventsListener` constructs
  `UIKeyPressMessage(597)` and posts it; the UI selects DTV/tuner
  (`ICE_VideoSelectSource` source 8 →
  `TunerManager::selectServiceForDisplay(1)`). The key-597 emitter is
  recovered; the tuner service/view contract remains open. The status-4/HPD
  policy is the cable-unplug path. The video-lost methods only set flags and
  call CEC; they do not select the tuner. Do not substitute HPD for the sleep
  trigger.
- `requestSelectServiceForDisplay` @ `0x00fb19f0` is the tuner service/decoder
  path; its full service-list, decoder-allocation and UI state contract is not
  recovered here. It takes the `NovoServiceViewController::getCurrentView`
  result (vtable `+0xc8`, `0x00f3781c`) and `this+0x90`, then reaches
  `TunerManager::requestServiceSelectForDisplay` @ `0x00f776b8` →
  `requestServiceSelectForDisplayInternal` @ `0x00f771e0` →
  `requestServiceSelectInternal` @ `0x00f7568c`. The stock path uses
  `EASIServiceList` @ `this+0x7c`, a `TunerRequestData`/decoder set and the
  service handle; an always-switch-to-TV shortcut would be wrong. The
  position→handle part is now implemented/tested in `common/tuner_view.c`;
  the select/mute/decoder/UI body is still open.
- The no-signal timeout, black-screen and standby policies
  (`HDMINoSignalTimeout`, `NoSignalCapability`, `NovoSignalController`) are
  separate; they were not exercised in this trace.
- `mod_source` currently implements the connect/disconnect HAL path only. The
  pure policy helper (`common/source_loss.c`, `check-source-loss`), the
  corrected `0xc000001` decoder (`common/source_hdmi_event.c`,
  `check-source-hdmi-event`), the injectable adapter
  (`common/source_hdmi_event_io.c`, `check-source-hdmi-event-io`), the HPD
  model (`common/source_hpd.c`, `check-source-hpd`) and the VIN provider
  (`common/source_vin.c`, `source.vin_status`, `source.video_status()` v2,
  `check-source-vin` and the fake-module service test) and the gated
  `inactive_source(position)` v3 worker (`source.inactive_fallback`,
  `source_path` fake-service order/gates) are tested. Not wired: the `mod_cec`
  0x9D route, the full tuner select/mute/decoder/UI body and the UI timing.
  Do not add a guessed fallback.
- On-TV evidence (read-only): the V4L2 capture produced one scaler event while
  stock stayed up; `oa-hpd-status` read `01 01 00 01 00` (HDMI1=1) and stayed
  unchanged through a 120 s Deck sleep/resume; `oa-hdmi-vin` recorded the
  rejected `video20`/`video24`/`video28` GETs and the working `video21` status.
  Staging was removed, no helper remained, `/conf/oa_boot` was absent and
  `oa_safe`/`coredumpnoreset` were present.
- A brief `gdbserver --attach` attempt, before the key-597 emitter was found,
  stopped stock long enough for the watchdog to reboot the set. Stock returned
  as PID 1775; no staging/helper remained. Do not repeat a debugger attach on
  stock; the CEC listener disassembly identified the emitter instead.

## Evidence

`artifacts/analysis/source-loss-2026-11-12/`:

- `aurora-source-disconnect-policy.txt` — canonical Ghidra decompile/asm for
  the navigator, `InputManager::handleExternalSourceDisconnected`, connected
  callback and `publishExternalSourceState`.
- `aurora-navigator-disconnect.txt` — navigator connected/disconnected thunks.
- `aurora-external-interface-callback.txt` — callback dispatcher and
  registration.
- `aurora-source-loss-thread.txt` — `ExternalAVSourceManager` message/thread
  path.
- `aurora-ui-source-loss.txt` — `NovoScreenViewController`,
  `SvgAppExternalAVSourceHandler`, status listener and `requestSelectExtSource`.
- `aurora-input-fallback.txt` — status conversion and adjacent input paths.
- `aurora-source-status-map.txt` — status conversion functions.
- `rea-aurora-source-fallback.json` / `.asm` — REA 6.9.0 Ghidra cross-check of
  `InputManager::handleExternalSourceDisconnected` @ `0x00fb1fbc`.
- `aurora-nebula-register.txt` — `AitHub::initialise` registration and callback
  storage.
- `aurora-nebula-status-change.txt` — `NEBULA_HandleExternalInterfaceStatusChange`
  and the navigator status switch.
- `aurora-avsource-base.txt` / `aurora-extav-vtables.txt` — `AVSourceManager`
  base and `ExternalAVSourceManager` vtable slots.
- `nondovi-extsrc-status.txt` — `NDP_ExtSrc_Get/SetConnectInfo`, sync info.
- `nondovi-viu-reconnect2.txt` — `_VIU_CheckReconnect`.
- `nondovi-extsrc-ctrls.txt` — `NDP_ExtSrc_g/s_ext_ctrls` control IDs.
- `nondovi-connectinfo-refs.txt` — `ndp_SRC_ConnectInfo` references.
- `hdmi-connect-port.txt` / `hdmi-s-input.txt` — HDMI RX connect/release/input
  call sites.
- `hdmiswitch-s-input.txt` — switch-driver `s_input` call site.
- `nondovi-sclr-events.txt` / `nondovi-ctrl-event-update.txt` /
  `nondovi-update-sclr.txt` — VPL scaler event callback, payload construction
  and `UpdateSCLR` caller.
- `nondovi-ctrl-event-callers.txt` / `nondovi-sclr-event-refs.txt` — callback
  registration/event references.
- `ntkndphdmi.dis` — HDMI RX driver disassembly with the event/subscribe
  functions.
- `v4l2_event_abi.c` — target-toolchain ABI check: event size 128, `u` offset
  8, subscription size 32, `VIDIOC_SUBSCRIBE_EVENT=0x4020565a`,
  `VIDIOC_DQEVENT=0x80805659`.
- `v4l2_ext_abi.c` — target ABI check: `v4l2_ext_control` 20 bytes, pointer
  at 12, `v4l2_ext_controls` 24 bytes, controls pointer at 20.
- `hdmi-event-live-2026-11-12.log` — V4L2 `0xc000001` capture. Its old probe
  `status=23` label is byte 7; the raw payload and section 6 are authoritative.
- `hpd-live-2026-11-12.log` — read-only `0x6712` HPD capture: HDMI1=1 with the
  Deck on TV; 120 s Deck sleep/resume left `01 01 00 01 00` unchanged.
- `hdmi-vin-get-attempts-2026-11-12.log` — earlier direct `0xa00967` GET
  attempts; its `video21` `EPROTO` predates treating `error_idx` as the vendor
  sentinel. `video20`/`video24`/`video28` still reject.
- `hdmi-vin-sleep-resume-2026-11-12.log` — working `video21` HDMI RX status
  capture: `w0=1` active, `w0=0/5` after the TV leaves HDMI1, back to `w0=1`
  on resume. This is the no-signal consequence, not the sleep trigger.
- `stock-aurora-source-loss-excerpt-2026-11-12.log` — ANSI-stripped stock log
  excerpt with private ID records removed. Shows `LOST SIGNAL` (VIU event 3),
  `ICE_VideoHide`, key 597, `ICE_VideoSelectSource` source 8 →
  `TunerManager::selectServiceForDisplay(1)`, and the resume `CEC 0x82` →
  source 17 (HDMI1).
- `cec-9d-key597.txt` — `SvgAppCECEventsListener::notifyCECDataMessageReceived`
  `0x9D` branch: `movw r1,#597`, `UIKeyPressMessage(597)`, then
  `UIFramework::postMessage(..., 1)`.
- `demoapp-key597.txt` — `DemoApp::preHandleKeyPressMessage`/`handleMessage`
  key-597 handling and the 0x255/0x25b/0x22d portal close branch.
- `signal-dialog.txt` / `signal-dialog-counter2.txt` — `SignalLostDialog`
  external-signal timer, counter and `endCurrentDialog`.
- `input-timeout.txt` — `InputManager::selectExtSource`,
  `deselectExtSource`, `handleTimeout`.
- `ndal3-v4l2info.txt` — `Hal_Video_Get_V4l2Info`/`Hal_Video_V4l2InfoInit`.
- `hdmi-g-ext-ctrls.dis` / `ex-plat-ext-ctrls.txt` /
  `ndp-extsrc-stringtouser.txt` — ext-control routing and class handling.
- `avsource-status-producers.txt` — ICE-side `AVSourceManager` stubs and
  `ExternalAVSourceManager` status message producers.
- `ice-hdmi-status.txt` — `_ice_CheckHdmiPlugStatus` and
  `ice_externalCheckCbStatus`.
- `ice-hdmi-position.txt` — `_ice_ext_GetHDMICallbackPosition`,
  `ice_external_GetHDMIARCPort`, `ice_GetHdmiPositionInSourceList`.
- `scart-callback.txt` / `extinterfacecallback.txt` — ICE callback mapping.
- `extstatus-chain.txt` — `publishExternalSourceState` and
  `InputManager::externalSourceStatusChanged`.
- `extavmgr-open.txt` — `openExtInterface`, `deviceStatusChange`, thread.
- `extvideo-controller.txt` — `ExternalVideoStatusController` and SCART
  registration.
- `video-lost-path.txt` — video-lost/ready navigator and `InputManager`
  methods.
- `plat-vin-slot.txt` / `plat-vin-stable.txt` — `plat_vin_slot_get_status`,
  `Hal_Vin_GetHdmiStatus` fields and `PLAT_VIN_UpdateTDLayerSourceStatus`.
- `ice-external-open.txt` / `ice-video-cb-callers.txt` — ICE external/video
  callback registration.
- `ndp-extsrc-g-ctrl.dis` — `NDP_ExtSrc_g_ext_ctrls` control dispatch.
- `tunermanager-select.txt` — `TunerManager::requestServiceSelectForDisplay`
  and internal/select decompile; position→handle and select vtable calls.
- `cec-9d-key597.txt` / `demoapp-key597.txt` — CEC 0x9D → UIKeyPressMessage(597)
  and the DemoApp key-597 handler.

Related: [CEC-LIVE-2026-11-01](CEC-LIVE-2026-11-01.md),
[CEC-SPEC-VS-AURORA-2026-10-30](CEC-SPEC-VS-AURORA-2026-10-30.md),
[SOURCE-PATH-2026-11-09](SOURCE-PATH-2026-11-09.md),
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md).
