# CEC 1.3a spec vs aurora's implementation — 2026-10-30

Side-by-side comparison. Spec side: **HDMI 1.3a Supplement 1 (CEC)**, the normative text (local copy
`/tmp/opencode/cec13a.txt`). Implementation side: `aurora.elf` (symbols intact) — decompiled
`NovoCECController::*`, `CECInterface::*`, `_ice_cec_Handler`, `isMessageValid`, plus the `.rodata`
tables. Everything below was read out of the shipped binary; nothing was measured on the wire.

## 0. First, the implementation is two layers

This matters for any comparison, because "aurora doesn't handle X" is often "ICE handles X".

```
ICE layer (Novatek, ice_cec.c)          policy layer (Vestel, aurora)
  _ice_cec_Handler                        NovoCECController::handleCECMessage
  ICE_CECInit / Open / SendData           CECInterface::read -> CECMessage -> publisher
  PLAT_CEC_* (MCU transport)              device table, ARC, audio mode, key map, UI
```

The ICE layer does framing, validation, and a few protocol replies (CEC version, physical address)
plus its own workarounds. The policy layer does the TV behaviour. Both were inspected.

## 1. Messages the TV sends

| opcode | message | spec role | where in aurora |
|---|---|---|---|
| 0x00 | `<Feature Abort>` | follower, when it can't/won't handle | `LAB_0102fdb8`: sent for any unhandled **directed** message, params `{opcode, reason}` |
| 0x32 | `<Set Menu Language>` | TV broadcasts its menu language, answers `<Get Menu Language>` | `sendCecMessage(…, 0xf, 0x32, 3, …)` — broadcast to 15 |
| 0x46 | `<Give OSD Name>` | asks a device its name | `getHDMIName()` on `0x84` and on input switch |
| 0x70 | `<System Audio Mode Request>` | as initiator | present (SAM path) |
| 0x71 | `<Give Audio Status>` | as initiator | `sendCecMessage(…, la, 0x71, 0, 0)` |
| 0x7D | `<Give System Audio Mode Status>` | as initiator | `sendCecMessage(…, la, 0x7d, 0, 0)` |
| 0x82 | `<Active Source>` | TV broadcasts **0.0.0.0** when it takes the screen itself | `LAB_010302e8`: `uVar15=0x82`, 2-byte PA `0x0000`, broadcast |
| 0x83 | `<Give Physical Address>` | verify a device's PA | `sendCecMessage(…, la, 0x83, 0, 0)` |
| 0x84 | `<Report Physical Address>` | reply to 0x83, and announce itself | `LAB_0103036a`: 3 bytes (PA hi, PA lo, device type) |
| 0x85 | `<Request Active Source>` | after wake, to find the active source | policy layer + the ICE LA-4 workaround |
| 0x8F | `<Give Device Power Status>` | before waking/playing | `sendCecMessage(…, la, 0x8f, 0, 0)` |
| 0x90 | `<Report Power Status>` | reply to 0x8F | `local_40[0] = this[0x4c]`, 1 byte |
| 0xC1/0xC2/0xC3 | ARC replies / request | initiator side of ARC | `sendCecMessage(…, 0xc1/c2/c3, 0, 0)` |

## 2. Messages the TV accepts (the dispatch, verbatim from `handleCECMessage`)

`CVar1` is `CECMessage[0x0c]`, i.e. the opcode. Every entry below has its parameter check read too.

| opcode | message | spec | aurora handling | verdict |
|---|---|---|---|---|
| 0x00 | `<Feature Abort>` | clear the pending action | clears the device's init/abort state | ok |
| 0x04 | `<Image View On>` | wake TV, switch to the source | wakes, selects source, buffers `Active Source` | ok |
| 0x0D | `<Text View On>` | as above, also clear menus | same plus menu clear; aborts with reason 1 when not allowed | ok |
| 0x36 | `<Standby>` | TV may enter standby | only from the active device, and only if own power state ≠ 3 (in transition to standby) | ok |
| 0x44 | `<User Control Pressed>` | RCP | length must be 1 and sender must be the active device; 0x41–0x43 (vol up/down/mute) mapped via a table; others forwarded | ok |
| 0x45 | `<User Control Released>` | RCP | stops the key-press timer, forces 0x2f2 | ok |
| 0x47 | `<Set OSD Name>` | store name, ≤14 ASCII, directed only | length check `len-1 ≤ 13`, truncates to `len`, stores per LA | ok |
| 0x71 | `<Give Audio Status>` | reply with `<Report Audio Status>` | replies 0x7A with volume + mute bit | ok |
| 0x72 | `<Set System Audio Mode>` | enable/disable SAM | sets audio mode, replies 0x7E when needed | ok |
| 0x7A | `<Report Audio Status>` | update UI volume | updates volume dialog state | ok |
| 0x7E | `<System Audio Mode Status>` | — | clears/sets SAM | ok |
| 0x80 | `<Routing Change>` | length **4** (old PA + new PA) | length must be 4, compares both PAs | ok |
| 0x82 | `<Active Source>` | length **2**, sender ≠ 0 | length 2, sender ≠ 0, switches input | ok |
| 0x83 | `<Give Physical Address>` | reply 0x84 | replies 0x84 with PA + device type | ok |
| 0x84 | `<Report Physical Address>` | length **3**; record LA↔PA↔type | length 3, sender ≠ 0/15, stores PA + device type + video group, then asks the name | ok |
| 0x85 | `<Request Active Source>` | nobody-by-default → broadcast own 0.0.0.0 | if current input isn't HDMI, broadcasts `Active Source` 0x0000 | ok |
| 0x8F | `<Give Device Power Status>` | reply 0x90 | replies 0x90 from `this[0x4c]` | ok |
| 0x90 | `<Report Power Status>` | length **1** | length 1, stores the device's power state | ok |
| 0x91 | `<Get Menu Language>` | reply 0x32 | handled | ok |
| 0x9D | `<Inactive Source>` | source went away | clears the active source | ok |
| 0xA3 | `<Report Short Audio Descriptor>` | EDID/SAD bookkeeping | consumed for ARC/EDID | ok |
| 0xC0 | `<Initiate ARC>` | start ARC | ICE special-cases it, policy sets ARC and replies 0xC1 | ok |
| 0xC5 | `<Terminate ARC>` | stop ARC | replies 0xC2 | ok |

**Not present in the dispatch** (and therefore answered with `<Feature Abort>` reason 0 if directed):
`0x8D <Menu Request>` / `0x8E <Menu Status>` (Device Menu Control), `0x64 <Set OSD String>`,
`0xA0 <Vendor Command With ID>`, `0x89 <Vendor Command>`, `0x87 <Device Vendor ID>`,
`0x9E/0x9F <CEC Version>` / `<Get CEC Version>`, and everything in the recorder/tuner/timer/deck
features (`0x09`–`0x1B`, `0x33`–`0x35`, `0x41`–`0x43`, `0x97`–`0xA2`).

Two of those need a footnote:

* **CEC Version (0x9E/0x9F)** *is* handled — but in the ICE layer, not the policy layer: `ice_cec.c`
  has the debug strings `CEC Get CEC Version` and `CEC CEC Version`. So the gap is only apparent.
* **`0x8D <Menu Request>`** really is absent. Device Menu Control is an *optional* feature in 1.3a,
  so a Feature Abort is legal — but it does mean a device's own menu can't be surfaced via CEC.
* `setVendorIDE(LA, id)` exists, so vendor IDs are at least stored; `0xA0` handling is probably in ICE.

## 3. Behaviour the spec demands, and whether it's there

| spec requirement | clause | aurora | verdict |
|---|---|---|---|
| must not answer a broadcast with a directed reply | CEC 6/7 | `if (*(int *)(param_1 + 8) != 0xf)` guards the abort reply | conformant |
| feature abort with the right reason | CEC Table 29 | reason 0 for unrecognised, reason 1 ("not in correct mode") on 0x0D paths; per-device reasons via `setFeatureAbortReason` | conformant |
| validate operand lengths | each message's table | every branch checks `CECMessage[0x1c]` before using params (0x80=4, 0x82=2, 0x84=3, 0x90=1) | conformant, unusually strict |
| buffer `<Active Source>` while powering up | CEC 13.1.2 | `isActiveSourceMessageReceived` / `setActiveSourceMessageReceived` state | conformant |
| TV broadcasts `Active Source` 0.0.0.0 for its own screen | CEC 13.2.2 | 0x85 handler broadcasts PA 0x0000 when the current input isn't HDMI | conformant |
| store the OSD name and use it in menus | CEC 13.11.2 | `setHDMIName`, preferred over user + factory names | conformant |
| `<Give OSD Name>` only to a device that just appeared | CEC 13.11.2 | sent on 0x84 *and* on input switch when the cache is empty | conformant (superset, allowed) |
| only the device at 0.0.0.0 may be LA 0 | CEC 10.2 | `PLAT_CEC_GetLogicalAddress` / `SetLogicalAddress`; LA 0 fixed | conformant |
| `Report Physical Address` on new PA | CEC 10.1 | replies to 0x83; startup broadcast not found in this layer (likely ICE) | inconclusive |

## 4. Where the implementation goes beyond / around the spec

These are not deviations — the spec leaves room — but they are the parts a reviewer should know:

1. **LA-4 workaround** (`_ice_cec_Handler`): on `<Set OSD Name>` *or* `<Report Power Status = On>`
   from logical address 4, broadcast `<Request Active Source>`. The spec allows the TV to ask at any
   time; this just makes it happen for devices that never send `<Active Source>`.
2. **`==WA== Device reply address` rewrite** (`ICE_CECSendData`): rewrites the *destination* of
   `0xC3`/`0xA4`/`0x70` frames to a remembered "device reply address" when it differs. That is not
   spec behaviour at all — it papers over a device (or MCU) that answers from the wrong address.
   `0xC0` also stops `0xC1` from being sent (`==WA== Receive 0xC0 Stop sending 0xC1!`).
3. **`ForceAMPInitial`** (`_ice_CEC_ForceAMPInitialTask`): a retry loop that re-initialises an
   external amplifier when the audio-system handshake doesn't converge.
4. **Name cache is RAM-only**: the spec says "stored and used in any future on screen references",
   not "persisted". Re-learned every boot. Conformant but explains the label reverting to "HDMI 1".

## 5. Summary

* Coverage of the **TV-relevant** 1.3a feature set is essentially complete: One Touch Play, Routing
  Control, Device OSD Name Transfer, Device Power Status, System Audio Control/ARC, Remote Control
  Pass Through, Vendor ID storage. Missing: Device Menu Control (optional) and OSD Display's
  `<Set OSD String>` (optional).
* It is **stricter than the spec**: operand lengths are validated on every branch, broadcast replies
  are suppressed correctly, and unhandled opcodes get a proper Feature Abort rather than silence.
* The interesting part is not the protocol, it is the **three workarounds** (LA-4, destination
  rewrite, amp re-init). They tell you exactly which third-party devices misbehaved with this
  chassis — and they are the places where a device like the Steam Deck could get surprising
  behaviour.
* Nothing found contradicts the earlier conclusion: `Set OSD Name` handling, the 14-byte clamp, and
  the directed-only rule are all exactly per CEC 13.11.

## 6. What this implies for our own work

* We can trust the TV to behave correctly as a CEC *follower*; if a device does something odd, the
  workarounds are the place to look, not the dispatcher.
* If we ever add CEC behaviour in a native shell, the TV-side routine set above is the reference for
  the minimal correct set, and the spec tables give the operand lengths that aurora already enforces.
* The one measurable question left is whether the Steam Deck answers `<Give OSD Name>` — everything
  else on the TV side is confirmed present.
