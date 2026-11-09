# Staged cold-boot design — 2026-11-09

Status: **design and safety pieces implemented and host-tested; not deployed.
The native boot path stays disabled (`/conf/oa_safe` present,
`/conf/oa_boot` absent).** This follows the failed power-on attempt in
[BOOT-HOOK-COLD-ATTEMPT-2026-11-09](BOOT-HOOK-COLD-ATTEMPT-2026-11-09.md).

## Problem

The installed `/conf` hook can start the replacement on the real boot path,
and `oa-supervise` falls back to vendor aurora on user-space failure. That was
not enough: the failed attempt hung the set before the network came up, and
neither the supervisor nor SSH could recover it. Two gaps:

1. A kernel/MCU hang cannot run any user-space recovery.
2. Starting every hardware owner (DRM/OSD, panel, SPI/NIM, tuner, audio) at
   once makes a reset or hang unattributable.

## Recovery model

Three independent layers, all before the risky hardware init:

```text
MCU watchdog  ──►  external feeder (oa-wdtfeed, separate process)
                        │  stops feeding if the SoC stops scheduling it
                        ▼
attempt gate  ──►  oa-boot-gate.sh (in the boot entry, /conf state)
                        │  counts boots that never reached stability
                        ▼
supervisor    ──►  oa-supervise --wdt-keep --gate /conf
                        │  vendor fallback on user-space failure
```

- **Feeder**: a daemonized process re-arms the MCU watchdog every 3 s with the
  recovered `0xae` IPC (`FIPC_set_watch_dog`). If the kernel or MCU hangs, it
  cannot run and the MCU resets the set. It is opened before daemonizing, so a
  missing IPC node is a fail-closed error for the boot entry.
- **Attempt gate**: `prepare` evaluates the previous native boot. A token left
  by a boot that never reached stability counts as a failure; after three,
  `oa_boot` is removed. `start STAGE` writes a new token. This turns a hang +
  watchdog reset into an automatic rollback instead of a boot loop.
- **Supervisor**: with `--wdt-keep` it never disables the watchdog (the feeder
  owns it). On the stable milestone it clears the token and writes
  `oa_last_ok`. On fallback it writes `oa_wdt_stop` and re-arms the watchdog
  through `wdtctl`, so the vendor stack can own it again.

Failure matrix:

| failure | result |
|---|---|
| bundle missing/incomplete, manifest bad | boot entry execs stock before any hardware |
| boot gate blocked (`oa_safe`, no `oa_boot`, 3 failures) | stock |
| watchdog feeder missing or IPC node unusable | stock (fail closed) |
| child exits or misses heartbeat/startup deadline | supervisor kills it, records failure, stops feeder, execs stock |
| third recorded failure | supervisor removes `oa_boot` |
| kernel/MCU hang | feeder stops, MCU resets; next boot counts the token |
| operator wants stock back | `touch /conf/oa_safe` (immediate fallback), remove `oa_boot`, reboot |

## Stage ladder

`tools/boot/stage-profile.sh STAGE` derives a config from
`openaurora-cold.conf`. Later stages are cumulative; source, audio-source, CEC
and the amp stay gated in every stage.

| stage | adds | flags |
|---|---|---|
| 0 | DRM + OSD reinit + UI only | `plat.drm_init=1`, `display.cold_init=1`, `display.osd_reinit=1`; panel/SPI/tuner/I2S off |
| 1 | panel power sequence | `panel.standby=1`, `panel.power_on_start=1` |
| 2 | SPI | `spi.enable=1` |
| 3 | NIM-DTV tuner | `tuner.hw_init=1` |
| 4 | audio clock | `audio.i2s_mclk=1` |

`/conf/oa_stage` selects the stage. The boot entry uses
`openaurora-stageN.conf` when present, otherwise `openaurora.conf`.

## Boot entry

`tools/boot/run-aurora.sh` is the intended appmgr `idtv` entry. It is
fail-closed:

```text
validate bundle + manifest
rm /conf/oa_wdt_stop
oa-boot-gate.sh /conf prepare        # blocked -> exec stock
oa-boot-gate.sh /conf start STAGE    # write the attempt token
oa-wdtfeed --node /proc/nvtstbcipc --interval 3 --timeout 10 \
           --stop /conf/oa_wdt_stop --log /conf/tvhack.log   # fail -> exec stock
exec oa-supervise --heartbeat ... --ready ... --wdt /buffer/wdtctl \
     --fallback /vendor/com.nvt.idtv/aurora.elf --gate /conf --wdt-keep \
     -- openaurora --modules ./modules --config openaurora-stageN.conf
```

On the set, the installed `/conf/remotelf/aurora.elf` hook keeps its existing
`oa_boot`/`oa_safe` outer gate; its native branch becomes:

```sh
if [ "$OA_NATIVE" = 1 ]; then
  exec /buffer/openaurora/run-aurora.sh
fi
exec /vendor/com.nvt.idtv/aurora.elf
```

The wrapper then re-checks the gate, starts the feeder and hands off to the
supervisor. This hook change is **not deployed**.

## Implemented and tested this session

- `tools/mcu/oa-wdtfeed.c` — feeder; `check-wdtfeed` verifies the exact
  6-byte `{0,0xae,hi,lo,1,0xae}` packet, the beats limit, the stop file and
  bad arguments/node.
- `tools/boot/oa-boot-gate.sh` — `check-boot-gate` covers blocked/allowed
  states, token write, uncleared-token counting, three-failure disable,
  clear/reset, `oa_safe`, malformed counters and invalid stages.
- `tools/boot/oa-supervise.c` — `--wdt-keep` and `--gate`; `check-supervise-boot`
  verifies no `disable` with the feeder, gate clear on stability, default
  disable behavior, and the three-failure disable.
- `tools/boot/stage-profile.sh` — `check-stage-profiles` verifies the ladder,
  closed gates and absolute UI/broker paths for stages 0..4.
- `tools/boot/run-aurora.sh` + `check-appmgr-entry` — stage config selection,
  gate prepare/start, feeder start, fail-closed gate/feeder paths and the
  supervisor arguments.
- `tools/boot/aurora-boot-hook.sh` — the installed hook with diagnostics, the
  bounded `/buffer` wait and the native branch handing off to
  `run-aurora.sh`.
- `tools/boot/deploy-staged-cold.sh` + `check-deploy-staged-cold` — dry-run /
  apply / restore; refuses without `coredumpnoreset`, `/buffer/wdtctl`, a
  verified manifest or the selected stage config; backs up the bundle and hook;
  installs the hook template, `oa_stage`, `oa_boot` and removes `oa_safe`.
- `cold-replacement-bundle` now contains `oa-wdtfeed`, `oa-boot-gate.sh`,
  `aurora-boot-hook.sh`, `deploy-staged-cold.sh` and
  `openaurora-stage0..4.conf`; the ARM bundle manifest verifies.

## Deployment runbook (not executed)

Prepare on the host, then run only with explicit owner authorization:

```sh
# 1. copy the bundle to a fresh directory on /buffer (host -> TV, verified)
tar -czf /tmp/opencode/oa-stage0.tgz -C openaurora/build-tv-v3/cold-replacement .
#    ssh: mkdir /buffer/openaurora-staged; tar -xzf -  (separate verify step)
cd /buffer/openaurora-staged && sha256sum -c manifest.sha256

# 2. preflight and dry run (no changes)
./openaurora --check --modules ./modules --config ./openaurora-stage0.conf
sh deploy-staged-cold.sh --bundle /buffer/openaurora-staged --stage 0

# 3. apply (backs up /buffer/openaurora and the hook first)
sh deploy-staged-cold.sh --bundle /buffer/openaurora-staged --stage 0 --apply

# 4. power-cycle the set (owner-approved). The current stock session is
#    unaffected until then.
```

Expected on the next boot (stage 0):

```text
/conf/tvhack.log: oa: pre ... / oa: post ... / oa: decision native=1
/conf/tvhack.log: run-aurora /buffer/openaurora stage=0
/conf/tvhack.log: oa-wdtfeed: beat 1, watchdog 10s
/conf/tvhack.log: oa-supervise: watchdog kept armed (external feeder)
/conf/tvhack.log: oa-supervise: host loop alive, first present acknowledged
/conf/tvhack.log: oa-supervise: boot attempt cleared
/conf/tvhack.log: oa-supervise: stable; failure streak cleared
/conf/oa_last_ok: ok
/conf/oa_fail: 0
```

Verify with SSH (the hook's dropbear stays up): `sh /buffer/openaurora/oa-boot-gate.sh /conf status`,
`cat /conf/oa_last_ok`, `ps` for `openaurora`/`oa-wdtfeed`, and a read-only
fb capture. Then obtain owner visible confirmation and keep it running for
several minutes.

Failure handling: a reset/hang stops the feeder, the MCU resets the set, and
the next boot counts the attempt; three unproven boots remove `oa_boot`. If
SSH is reachable but the UI failed, `touch /conf/oa_safe` selects the vendor
fallback immediately.

Restore:

```sh
sh /buffer/openaurora/deploy-staged-cold.sh --restore --apply
# or: touch /conf/oa_safe; rm -f /conf/oa_boot
reboot
```

Then verify stock (`appmgr`, `aurora.elf`, fb1 1920x2160, `oa_safe` present,
`oa_boot` absent). The USB recovery-stability stick remains the last resort.

## Acceptance procedure (per stage, with explicit owner authorization)

1. Deploy the bundle to `/buffer/openaurora` and verify `manifest.sha256`.
2. `echo N > /conf/oa_stage`; keep the `openaurora-stageN.conf` for that stage.
3. Confirm `/conf/oa_boot` exists, `/conf/oa_safe` absent, feeder/gate
   executable and `/conf/coredumpnoreset` present.
4. Power-cycle. Verify: SSH/telnet reachable, `oa_last_ok` written,
   `/tmp/openaurora.ready` fresh, expected stage modules started, fb capture.
5. Keep it running for the stable window plus several minutes, then verify the
   supervisor did not fall back (`/conf/oa_fail` = 0, no `exec vendor fallback`
   in the log) and obtain owner visible confirmation.
6. Only then advance `/conf/oa_stage`. On any reset/hang, the gate counts it;
   three unproven boots disable the native path automatically.
7. Restore: `touch /conf/oa_safe`, `rm -f /conf/oa_boot`, reboot; verify stock.

## Limits

- No TV acceptance yet: the feeder's IPC is packet-tested only; the real
  `/proc/nvtstbcipc` protocol comes from the recovered `wdtctl`/`pcbconfig`
  path used earlier on the set.
- The gate cannot distinguish a kernel hang from an owner power-off inside the
  stable window; both count as unproven. Three is deliberate.
- The installed hook still needs the small native-branch change; it is not
  deployed and the path remains disabled.
- Panel/DRM ownership on a true cold boot is still unproven; that is what the
  staged runs must establish, one owner at a time.
