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
5 s blue-background/UI timing are still open.

## Limits and next work

- The exact HDMI hotplug event source (HPD, +5V/EDID, or ICE status) and the
  adapter that registers the navigator callback are not fully traced.
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
- `mod_source` currently implements the connect/disconnect HAL path only. A
  pure policy helper (`common/source_loss.c`, `check-source-loss`) is tested,
  but it is not wired: the source-loss event provider and the tuner service/
  view re-tune are still needed. Do not add a guessed fallback.
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

Related: [CEC-LIVE-2026-11-01](CEC-LIVE-2026-11-01.md),
[CEC-SPEC-VS-AURORA-2026-10-30](CEC-SPEC-VS-AURORA-2026-10-30.md),
[SOURCE-PATH-2026-11-09](SOURCE-PATH-2026-11-09.md),
[COLD-START-SEQUENCE-2026-11-10](COLD-START-SEQUENCE-2026-11-10.md).
