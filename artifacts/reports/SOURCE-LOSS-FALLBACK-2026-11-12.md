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
- if `this+0x94 == -1`, checks interface `0x13` (`isInterfaceEnabled` @
  `0x00fafdf0`), and when the service list is non-empty selects the saved tuner
  service **`this+0x90`**:

```text
0xfb206a  ldr r0,[r4,#0x14]
0xfb206c  ldr.w r8,[r4,#0x90]
0xfb2070  bl 0x00f70540  ; TunerManager::getNovoServiceViewController
0xfb2074  ldr r2,[r0,#0]
0xfb2076  ldr.w r3,[r2,#0xc8]
0xfb207a  blx r3
0xfb208e  bl 0x00fb19f0  ; requestSelectServiceForDisplay(this, r8, true, ...)
```

- otherwise, if `this+0x94 != -1`, selects the previous external source
  **`this+0x94`** and clears it:

```text
0xfb2048  bl 0x00fb1c50  ; requestSelectExtSource(this, this+0x94, true)
0xfb204c  str.w r5,[r4,#0x94]   ; this+0x94 = -1
```

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
- The primary fallback is the saved tuner service `this+0x90`; if that is not
  available, the previous external source `this+0x94`.
- The branch requires `this[0x88] == 0`, a matching video group, a non-empty
  service list for the tuner path, interface `0x13` enabled, and `this+0x98`
  set.
- `0x9D Inactive Source` is not the trigger; it does not clear the active source
  in this path. `0x36 Standby` is also not the trigger (the TV stayed on).
- REA's Ghidra provider recovered the same function and the same calls; its
  assembly at `0xfb2004`, `0xfb2048` and `0xfb208e` matches the canonical Ghidra
  dump exactly.

## Limits and next work

- The exact HDMI hotplug event source (HPD, +5V/EDID, or ICE status) and the
  adapter that registers the navigator callback are not fully traced.
- `requestSelectServiceForDisplay` @ `0x00fb19f0` is the tuner service/decoder
  path; its full service-list, decoder-allocation and UI state contract is not
  recovered here. A "always switch to TV" shortcut would be wrong.
- The no-signal timeout, black-screen and standby policies
  (`HDMINoSignalTimeout`, `NoSignalCapability`, `NovoSignalController`) are
  separate; they were not exercised in this trace.
- `mod_source` currently implements the connect/disconnect HAL path only. This
  report does not add a guessed fallback; implementing it needs the tuner
  service/decoder contract and the source-loss event provider, gated off by
  default.
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
