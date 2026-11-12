# HDMI source-loss fallback — 2026-11-12

Status: current
Last verified: 2026-11-12
Evidence class: firmware instructions plus REA cross-check and owner-reported
stock behavior; no new on-TV test.
Corrects: the 2026-11-12 CEC-LIVE hypothesis that `0x9D Inactive Source`
caused the Deck-sleep tuner fallback. `0x9D` returns unless the TV is already in
the standby transition; it does not select a tuner.

## Observation

The owner tested stock suspend/resume with the Steam Deck on HDMI1:

- Deck sleep: the TV stayed on and left HDMI1 for a tuner channel.
- Deck resume: the TV switched back to HDMI1.

The resume side is consistent with `0x82 Active Source` calling
`selectSource`. The sleep side is the **external-interface disconnect**
fallback below, not CEC `0x9D` and not `0x36 Standby`.

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
external source (TV/tuner). The common Deck-sleep-from-TV path therefore has
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

### 6. V4L2 event provider

The userspace notification is a private V4L2 event on `/dev/video21`
(`ndp-hdmi`, the live node name):

- `_ndp_hdmi_core_vidioc_subscribe_event` @ `0x0000052c` accepts only event
  type `0xc000001`; it calls `VPL_SCLR_CTRL_Event_Subscribe` and
  `v4l2_event_subscribe`.
- `VPL_SCLR_CTRL_Event_RegisterCB` @ `0x00637538` stores `event_cb` at
  `0x00bb3e64 + 4`; `eventEnable` at `0x00bb3e64 + 0` is gated by bit 1.
- `_CTRL_EVENT_Update` @ `0x006394bc` builds a 28-byte payload and calls
  `event_cb(0xc000001, &payload)`. Its caller
  `VPL_SCLR_CTRL_UpdateSCLR` @ `0x0063967c` passes `(source, arg == 6)`.
- Payload layout: source type at `[0..3]` (little-endian), `VidSetting+0x86`
  at `[4..5]`, `VidSetting+0x0c` at `[6]`, the `(arg == 6)` status byte at
  `[7]`, and scaler timing fields at `[8..0x1b]`.
- `_ndp_hdmi_core_event_callback` @ `0x0000150c` copies those 28 bytes into
  the V4L2 event data and calls `v4l2_event_queue`.

Address convention: the `ntkndphdmi.ko` addresses are `.text` section offsets
(objdump/nm); the `ntkdriver_nondovi.ko` addresses are Ghidra runtime addresses
(`.text` offset + `0x10228` in this import).

Pure decoder: `include/openaurora/source_hdmi_event.h` /
`common/source_hdmi_event.c`. `make check-source-hdmi-event` covers the
28-byte bound, little-endian source type, status byte, raw preservation and
short/null rejection. The injectable adapter
`include/openaurora/source_hdmi_event_io.h` / `common/source_hdmi_event_io.c`
uses the target ABI (`sizeof(v4l2_event)=128`, data at offset 8,
`VIDIOC_SUBSCRIBE_EVENT=0x4020565a`, `VIDIOC_DQEVENT=0x80805659`) and
`make check-source-hdmi-event-io` covers open/subscribe, dequeue decode,
EAGAIN/wrong-type/error and cleanup. The full host suite passes. The adapter is
not wired to a worker, and the live `/dev/video21` capture requires stock
stopped and was not run; stock owns the node and consuming its events would
interfere. The status byte's role (video ready/lost vs connect/disconnect) is
not proven; do not map it to the NEBULA/`ExternalSourceStatus` values without a
captured event.

## What this establishes

- Stock sleep fallback is an **external-interface disconnect** policy, gated by
  the disconnected source's video group relative to `this+0x68`.
- On external connect, `this+0x94` saves the previous external source index
  (`-1` for TV) and `this+0x90` the tuner navigation position. On disconnect,
  `+0x94` is restored first; `+0x90` is the tuner-position path only when
  `+0x94 == -1`.
- The branch requires `this[0x88] == 0`, a matching video group, a non-empty
  service list for the tuner-position path, interface `0x13` enabled, and
  `this+0x98` set.
- `0x9D Inactive Source` is not the trigger; it does not clear the active source
  in this path. `0x36 Standby` is also not the trigger (the TV stayed on).
- REA's Ghidra provider recovered the same function and the same calls; its
  assembly at `0xfb2004`, `0xfb2048` and `0xfb208e` matches the canonical Ghidra
  dump exactly.

## Pure policy helper

`include/openaurora/source_loss.h` and `common/source_loss.c` transcribe the
recovered decision logic only. The helper:

- saves the previous external index and the tuner position on external connect
  and sets `auto_switched`;
- on a matching disconnect, a set `flag_88` is only cleared;
- otherwise restores the previous external index first, or returns the tuner
  position when `previous_external == -1` and `tuner_available`;
- performs no source select, tuner re-tune, HAL, device or UI I/O.

`make check-source-loss` covers TV→HDMI→tuner, external→external→tuner,
`flag_88`, `auto_switched`, group mismatch, invalid arguments and idempotent
connect. The full host suite passes. The helper is **not wired** into
`mod_source`: the external-interface event provider, the
`requestSelectServiceForDisplay`/`TunerManager` service/view re-tune and the
5 s blue-background/UI timing are still open. Section 5 identifies the stock
provider entry and status block, but not the hardware event ID/caller; do not
wire a guessed V4L2 event.

## Limits and next work

- The HDMI userspace notification is identified as V4L2 event `0xc000001` on
  `/dev/video21`; the payload decoder and injectable adapter are tested. The
  live adapter/worker and live capture are not wired or run. The status byte's
  role and any `NDP_ExtSrc_SetConnectInfo` caller outside the analyzed RX/switch
  paths remain open. In `ntkndphdmi.ko`, `SetConnectInfo` is called from
  `tztv_hdmi_int_switch_connect_port` @ `0x1070`,
  `_ndp_hdmi_core_release_func` @ `0x42b4` and
  `_ndp_hdmi_core_vidioc_s_input` @ `0x46e0`; `ntkndphdmiswitch.ko` calls it
  from `_ndp_hdmi_switch_vidioc_s_input` @ `0x7ac`. The 0x74-byte
  `ndp_SRC_ConnectInfo` per-port flags are at `0x0`, `0xc`, `0x14`, `0x1c`,
  `0x28`, `0x34`, `0x40`; the V4L2 controls
  `0xa00966`/`0xa00968`/`0xa0099f`/`0xa00967`/`0xa009ab` do not expose the
  block in the analyzed path.
- `requestSelectServiceForDisplay` @ `0x00fb19f0` is the tuner service/decoder
  path; its full service-list, decoder-allocation and UI state contract is not
  recovered here. It takes the `NovoServiceViewController::getCurrentView`
  result (vtable `+0xc8`, `0x00f3781c`) and `this+0x90`, then reaches
  `TunerManager::requestServiceSelectForDisplay` @ `0x00f776b8` →
  `requestServiceSelectForDisplayInternal` @ `0x00f771e0` →
  `requestServiceSelectInternal` @ `0x00f7568c`. The stock path uses
  `EASIServiceList` @ `this+0x7c`, a `TunerRequestData`/decoder set and the
  service handle; an always-switch-to-TV shortcut would be wrong.
- The no-signal timeout, black-screen and standby policies
  (`HDMINoSignalTimeout`, `NoSignalCapability`, `NovoSignalController`) are
  separate; they were not exercised in this trace.
- `mod_source` currently implements the connect/disconnect HAL path only. The
  pure policy helper (`common/source_loss.c`, `check-source-loss`), the
  `0xc000001` payload decoder (`common/source_hdmi_event.c`,
  `check-source-hdmi-event`) and the injectable V4L2 adapter
  (`common/source_hdmi_event_io.c`, `check-source-hdmi-event-io`) are tested.
  Not wired: a worker/live capture (stock owns `/dev/video21`) and the tuner
  service/view re-tune. Do not add a guessed fallback.
- No TV access. The owner's suspend/resume observation is behavioral
  confirmation, not an independently captured hotplug trace.

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

Related: [CEC-LIVE-2026-11-01](CEC-LIVE-2026-11-01.md),
[CEC-SPEC-VS-AURORA-2026-10-30](CEC-SPEC-VS-AURORA-2026-10-30.md),
[SOURCE-PATH-2026-11-09](SOURCE-PATH-2026-11-09.md),
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md).
