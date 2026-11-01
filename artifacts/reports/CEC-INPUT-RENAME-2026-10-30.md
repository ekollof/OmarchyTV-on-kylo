# Does CEC rename the inputs? — yes, learned and cached — 2026-10-30

> **Status: Static firmware name-cache policy. Not proof that the replacement implements it or that every external device answers.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Question: when a device advertises a name over CEC (the way a Steam Deck / dock does), does the TV
rename the HDMI input? Answer: **yes, but as a learn-and-cache, not a one-message rename.** The
advertised name is stored per CEC logical address in RAM, and it is then **preferred over both the
user's own custom name and the factory default** when the input name is produced.

All of this is static analysis of the shipped `aurora.elf` (symbols intact, dumped from our own NAND
image). No TV needed.

## The chain

```
device hotplug  ──► Report Physical Address (0x84)
                      ├─ setDevicePhysicalAddress(LA, PA)      (cross-links LAs sharing one PA)
                      ├─ setDeviceType(LA, device_type)
                      └─ getHDMIName(LA)  ──►  no stored name?  ──►  send  Get OSD Name (0x46)
                                                                       │
device answers  ──► Set OSD Name (0x47)  ───────────────────────────────┘
                      └─ setHDMIName(name, LA)   [≤ 14 chars]
                            ├─ store at this + LA*0x128 + 0x13c        (the UString name slot)
                            ├─ mirror onto the peer LA sharing the PA  (this + LA*0x128 + 0x150)
                            └─ input is HDMI & currently showing?
                                  └─ CIDSetInputName()  ──►  ICE_CIDSetInputName
                                                              ├─ global 0x80-byte input-name buffer
                                                              └─ PLAT_SAMBA_SetVideoInputName()
```

and when a name is *read*:

```
NovoExternalAVSourceNavigator::getExternalInterfaceName
   └─ InputManager::getExternalInterfaceName          (0x00fb09d4)
        ├─ 1. connector type == 7 (HDMI)  ──►  NovoCECController::getHDMIName(LA)
        │        → if it has a name, USE IT (and skip the rest)
        ├─ 2. UserPreferences::getSourceName(extIndex + 1)      ← the owner's custom label
        └─ 3. NovoSystemInformationProvider::getHdmi1Name() …    ← factory default
```

So the **priority is CEC name > user-defined name > factory name**. A device that advertises a name
overrides a hand-made "STEAMDECK" label in the source list. Only inputs whose connector type is 7
(HDMI1–4) participate.

## Details that matter

| thing | finding |
|---|---|
| opcodes | `Get OSD Name` **0x46** (TV → device, directed, no payload), `Set OSD Name` **0x47** (device → TV) |
| length | payload is truncated to the CEC maximum of **14 bytes** (`if (0xd < len - 1) reject`, then `getSubstring` to `min(len, strlen)`) |
| addressing | `isMessageValid` requires destination **≠ 0xF** for 0x47 — a *broadcast* Set OSD Name is dropped. Only a directed reply counts. |
| where stored | `NovoCECController` device table, stride **0x128**, field **+0x13c** (a `UString`). Per logical address (1..14). |
| peer mirroring | `setDevicePhysicalAddress` cross-links two LAs on the same physical address via **+0x150**, and `setHDMIName` copies the name to that peer — a device that claims two logical addresses (e.g. Playback + Audio System) gets the same name |
| PA → input | `{0x1000→3, 0x2000→4, 0x3000→9, 0x4000→12, 0x5000→13}` (`.rodata 0x152fa4c`); group IDs 3/4/9/0xc are HDMI1/2/3/4 in `getExternalInterfaceName`'s switch |
| persistence | **none.** The name lives in the `NovoCECController` object only. `clearDevice()` clears +0x13c and resets the input label (`CIDSetHDMIisCEC(false)` + re-push the default). After a reboot the set is back to "HDMI 1" and re-learns on the next hotplug. |
| side effect | `CIDSetInputName` is not the OSD label — it feeds the **CID/Samba** layer (`PLAT_SAMBA_SetVideoInputType/Number/Name`), i.e. content recognition gets told what is plugged in |
| auto-switch | the same path does the switching: `Report Physical Address`/`Image View On` → `NovoCECController::selectSource()` maps PA→external interface, `requestSelectExtSource(idx)`, `setSwitchedAutomatically(true)`, `CIDSetHDMIisCEC(true)` |

## The separate LA-4 workaround (not renaming)

`_ice_cec_Handler` (`ice_novatek/ice_cec.c`) contains a small state machine that watches for these
exact frames and, on match, **broadcasts `Request Active Source` (0x85)**:

* state 0 → `Set OSD Name (0x47) from logical address 4`
* state 1 → `Report Power Status (0x90) == ON from logical address 4`

Its initial value is `0xff` (idle) and the arm flag lives in `.bss`, so in production it is dormant —
it is a test-tool injector ("Pseudo From: 0: Set OSD Name, 1: Report Power Status"). `ICE_CECSendData`
sets state 1 whenever the TV itself sends `Give Physical Address (0x83)`, and there is a matching
"`==WA== Device reply address 0x%x to TV`" family that rewrites the *destination* of 0xC3/0xA4/0x70
frames — the usual Vestel workaround for a set-top box with sloppy CEC addressing.

It does **not** rename anything; don't confuse it with the rename path above.

## Practical answers

* **Steam Deck:** if its CEC implementation answers `Get OSD Name` with `Set OSD Name`, the input will
  show that string (≤ 14 chars). If it never answers 0x46, the TV keeps "HDMI 1" — the request is sent,
  but nothing changes without a reply. Note the reply must be **directed** at the TV, not broadcast.
* The name is re-requested after every boot / unplug, so it is normal for the label to fall back to
  "HDMI 1" until the device is re-detected.
* If we ever want a fixed label on the launcher's "Sources" list, remember CEC wins over
  `m_source_names`; renaming from our side would have to go through the same per-LA slot, or the
  device would have to stop advertising.

## Where things are

```
InputManager::getExternalInterfaceName        0x00fb09d4   (priority order, HDMI type 7 gate)
NovoCECController::setHDMIName                0x0102e52c   (store + live CID push)
NovoCECController::getHDMIName                0x0102e5e0   (read, or send 0x46)
NovoCECController::handleCECMessage           0x0102fce0   (0x47 branch at ~0x010303xx, 0x84 branch)
NovoCECController::selectSource               0x0102f5e0   (PA→input switch + rename + 0x46)
NovoCECController::clearDevice                0x0102f008   (drops name, restores default label)
NovoCECController::setDevicePhysicalAddress   0x0102ec68   (PA store + +0x150 peer link)
ICE_CIDSetInputName                           0x007d093c   (global buffer + Samba push)
_ice_cec_Handler                              0x008067bc   (LA-4 workaround → Request Active Source)
ICE_CECSendData                               0x00807274   (sets the workaround state on 0x83)
```
