# Bronlijst VIN select + picture commit — 2026-11-06

TV was off limits. Offline host control-socket tests with a fake Hal_Vin
fixture, plus the QEMU ARM UI stack with a fake VIN module. No TV,
`/dev/ntkdvif`, vendor HAL or input key was used.

## Decoded source path

`ICE_ExternalGetVideoDevice(index)` reads the video-device field at
descriptor `+0x00`; `ExternalAVSourceManager::selectExtSource` passes that
device to the video manager, and `PLAT_VIU_ChangeInput` reaches
`plat_vin_slot_connect` → `FUN_0092e8ac(type)` → the 26-entry table at
`0x013b9e78` → `Hal_Vin_SelectInput(fd, id)` + `Hal_Vin_SetStreamStart`.

The Bronlijst rows map to:

| row | source | video device | `m_current_source` | VIN id |
|---|---|---|---|---|
| 0 | TV | 0 (ATV path) | 0 | 1 |
| 1 | BACK AV | 15 | 3 | 1 |
| 2 | HDMI1 | 17 | 5 | 3 |
| 3 | HDMI2 | 18 | 6 | 3 |
| 4 | HDMI3 | 19 | 7 | 66 |
| 5 | YPbPr | 20 | 9 | 3 |
| 6 | VGA/PC | 16 | 10 | 2 |
| 7 | Wireless | 29 | 12 | no VIN slot |
| 8 | AV share/APM | 30 | 13 | no VIN slot |

`mod_source` still enforces `source.enable`; a disabled module returns -2 and
the UI does not claim a switch.

## UI transition

`ui_vendor` Bronlijst Select now performs one ordered transition:

1. look up `source` and call `select_type(video_device)` for rows with a VIN
   slot; no source service, `source.enable=0`, or a HAL failure aborts the
   selection and leaves the old source/picture untouched;
2. only after a successful VIN select, write `m_current_source` through the
   settings service;
3. call `picture_sync_index()` so the cards read that source's 0x240 picture
   slot;
4. emit the UI action and repaint.

WiDi/APM have no decoded VIN slot and are marked `-1`; they still update the
picture slot but do not call Hal_Vin.

## Verification

Host fake-VIN control test (`check-source-fake-ipc`) stages the real
`mod_source` linked against a fake Hal_Vin:

```text
ok source fake-VIN IPC: decoded ids, state query and source.enable gate
```

It asserts the exact ids for types 0/15/16/17/18/19/20 (1/1/2/3/3/66/3), the
`source-state` query, and that a fresh process with `source.enable=0` returns
`failed disabled` without a VIN select.

QEMU value check with the ARM fake source in the module list and
`source.enable=1`:

```text
ok qemu sub-launcher value: m_current_source=1 gives NATURAL/MAXIMUM/16:9
(132/129/54); ... source5 gives DYNAMIC/OFF (151/36); source-screen HDMI1
writes 5 and resyncs (151/36)
```

The source-screen step starts at Bronlijst row 2 (HDMI1), sends `key 53`
through the real control socket, asserts the fake VIN log contains
`select fd=100 id=3`, asserts `Settings.m_current_source=5`, and then
`open:sub_launcher` shows config 5 (DYNAMIC/OFF, 151/36 px) in the same
process.

Full suite and existing QEMU checks:

```text
full_check=0
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The VIN fixture is a log-only fake; real `/dev/ntkdvif` select/stream-start
  and TV hardware acceptance remain a separate gate. The owner has not
  authorized hardware switching.
- The `m_current_source`/picture write is atomic in the UI state; the vendor
  in-memory `UserPreferences`/PQ object is still not executed.
- WiDi/APM (types 29/30) have no decoded VIN slot; only the picture index is
  updated.
- No vendor Bronlijst/value-change captures exist while the TV is off limits.
