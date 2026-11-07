# Optical ALSA control diagnosis — 2026-11-07/08

Status: TV-side tool and controlled audio tests. The soundbar root cause is
**reproduced and reversible in the OPU-volume case**, but a takeover can also
leave sticky output-route controls that no ALSA write can reset. A stock reboot
is still the only recovery for that second state. The TV was stock
(`appmgr 1574`, `aurora.elf 1776`) after the final reboot.

## Tool

`openaurora/tools/audio/oa-alsactl.c`, built for ARM against the TV's
`/vendor/lib/libs/libasound.so.2`:

```text
oa-alsactl [--card N] list [FILTER]
oa-alsactl [--card N] get NAME [INDEX]
oa-alsactl [--card N] set NAME VALUE
oa-alsactl [--card N] setv NAME VALUE [VALUE...]
oa-alsactl [--card N] save FILE
oa-alsactl [--card N] restore FILE
```

It uses the ALSA control API only; no HAL, telemetry or network. The TV image
has no `amixer`/`alsactl`, so this fills that gap.
`ALSA_CONFIG_PATH=/vendor/etc/alsa.conf` is required.

The snapshot format is `name<TAB>index<TAB>type<TAB>count` followed by one
TAB-separated value field per element (`bytes` is one hex string). Earlier
snapshots concatenated the byte count and data, so `hw out mute` read as `100`
(count 100) instead of count 1, value `00`; the format now has a tab. Old
snapshots with byte controls are ambiguous and should not be restored.

`restore` now applies a whole multi-element vector with one `setv`-equivalent
write. The earlier per-element loop wrote every element with each value, which
is wrong for controls such as `nvt opu hw volume 0 12000 12000`.

## Healthy optical state (card0)

```text
nvt opu hw volume          int    0 0 0
main out source select     enum   2 (DTV DEC0)
main out source connect    enum   2 (DTV DEC0)
spdif bypass               enum   3
hw out mute                bytes  00 (count 1)
spdif region area          enum   0 (atsc)
hp/line out nvt enable     enum   0
hp/line out hp gain enable enum   1
```

The earlier claim `hw out mute = 0x10` was a parsing artifact; it is one byte
`0x00`. The four "optical" controls alone were never sufficient.

## Confirmed root cause: OPU hardware attenuation

On a healthy boot, writing the silent-state vector:

```text
setv "nvt opu hw volume" 0 12000 12000
```

made the soundbar silent immediately. Writing `0 0 0` restored audio **without
a reboot**. The control is attenuation-like (12000 is far below full scale on a
0..32768 control), not the UI volume. Healthy playback keeps it at `0 0 0`.

The takeover can leave this at `0 12000 12000`. `takeover-full.sh start` saves
the full card0 snapshot and extracts `nvt opu hw volume` together with the four
route controls; `restore` reapplies that file and then forces `0 0 0` as a
defensive final step. This path was exercised on the TV.

## Second failure: sticky output-route state

A later takeover/restore left OPU at `0 0 0` but the soundbar was still silent.
Diffing a known-good playback snapshot against the post-restore snapshot showed:

```text
spdif region area           healthy 0   takeover 1
hp/line out nvt enable      healthy 0   takeover 1
hp/line out hp gain enable  healthy 1   takeover 0
```

These are output-route/headphone-detect controls. Writing the known-good values
returned success (`rc=0`) but fresh reads still showed the takeover values, so
they are sticky in this driver state. A `reboot -f` restored all of them and the
soundbar. `Hal_Audio_Proxy_Init` opens `/proc/ksif` and the prelude/route owner
is the leading suspect, but the exact writer is not yet isolated.

Consequences:

- The takeover restore is not audio-complete. A reboot remains required when a
  takeover leaves this sticky route state.
- The audio gate stays open until either the writer is prevented (for example a
  prelude/module gate) or a driver-level reset that is safe under the stock
  owner is recovered.
- `nvt opu hw volume` restore is still worth keeping: it fixes the separate
  attenuation regression and is verified reversible.

## Full-card restore is unsafe

A full 181-control save/restore returns `EPERM` (read-only/status controls) and
was previously observed to change `main out source select` to 15. Only the
filtered critical set is used:

```text
nvt opu hw volume
main out source select
main out source connect
spdif bypass
hw out mute
```

## Evidence

- `/buffer/oa-audio-good-20261107/card0.txt` — healthy boot.
- `/buffer/oa-audio-good-play-20261107/card0.txt` — healthy playback.
- `/buffer/oa-audio-broken-20261107/card0.txt` — first post-takeover silent state.
- `/buffer/oa-audio-broken2-20261108.txt` — sticky-route silent state.
- Bundle `oa-full-20261108.tgz` sha256
  `14cbb34594a3c6cee37df9e0cbb17f6bac0ee3f4433b9c36336c1cba83406c64`;
  `oa-alsactl` sha256
  `250b7d25280befba7d78627b46c1b38855edc6ea226d3ceddce900341316f66b`.
- The OPU and route tests were audible checks by the owner; control values
  alone are not audio proof.

## Limits

- Exact writer of the sticky route controls is unresolved.
- The `spdif region area`/HP/NVT restore values are known-good observations on
  this firmware, not yet traced to a vendor call contract.
- Enum item labels are printed by this tool only as an aid; the ALSA 1.0.29
  library did not reliably return a label for the selected item in every case.
- TV speakers remain intentionally muted; this is not amp/speaker acceptance.

Report: ALSA-CONTROL-2026-11-07. Related: TAKEOVER-ACCEPT, AUDIO-ROUTING,
SPEAKER-GAIN, AUDIO-VOLUME.
