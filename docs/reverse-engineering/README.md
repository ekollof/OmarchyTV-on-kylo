# Reverse-engineering overview

This directory is the entry point to the firmware reverse-engineering evidence
for the Telefunken D65U546A / Vestel MB230 set. It is a navigation and
compilation layer: the authority remains the NAND dump, the extracted vendor
binaries, the Ghidra projects and the dated topic reports.

## Evidence rules

1. **Firmware first.** Recover the contract from the shipped binary (Ghidra and
   the exact ARM instructions) before probing. The TV confirms an understood
   contract; it is not used to discover one.
2. **Instructions over decompiler.** Saved Ghidra signatures are analysis
   metadata, often wrong about arguments, returns, struct-return pointers and
   Thumb/ARM state. Check the instructions at the cited address.
3. **Separate the evidence classes.** Host tests prove model/order/arithmetic;
   qemu proves ARM execution with a guest rootfs; a read-only on-TV query
   proves the live state; a controlled takeover proves the device effect only
   for what was actually observed. A framebuffer capture is not physical
   scanout proof.
4. **Date and correct.** Reports are dated records. [FINDINGS](../../FINDINGS.md)
   owns the correction map; a later report supersedes an earlier claim at the
   source, not by silent generalization.
5. **No redistribution.** Firmware, extracted libraries, dumps, private keys
   and third-party manuals stay local. See
   [artifacts/README.md](../../artifacts/README.md).

## Keeping this current

Follow the documentation hygiene rules in [AGENTS.md](../../AGENTS.md). When a
report, binary, hash, address or contract changes, update the matching document
here, add/remove the report in the canonical index, regenerate the topic map
(`sh tools/docs/gen-report-map.sh`) and run `sh tools/docs/check-docs.sh`.
Superseded reports are archived under `artifacts/archive/` with a top
`SUPERSEDED BY:` line; they are not current evidence.

## Where the evidence lives

`artifacts/workbench/`, `artifacts/dumps/` and `artifacts/payload/` are
**local-only** and absent in a fresh clone: they contain the NAND dump, vendor
binaries, Ghidra databases, toolchain and keys. Create them from your own TV
([../reproduce/README.md](../reproduce/README.md)); never publish or commit them.

| path | contents |
|---|---|
| `artifacts/dumps/tv_nand.bin` | 512 MiB NAND capture; source of truth (MD5 `36de1801…`) |
| `artifacts/workbench/rootfs/{root,vendor,vendor2}` | extracted rootfs/ap0/ap1 trees, used by the extractor and qemu |
| `artifacts/workbench/ghidra_*` | analysed Ghidra projects (map below) |
| `artifacts/workbench/aurora.dis` | objdump cache of `aurora.elf` (`-d --no-show-raw-insn`, ~189 MB) |
| `artifacts/workbench/ker*`, `venv/`, `velf/` | reconstructed kernels, UBI/disassembly tooling |
| `artifacts/reports/` | 338 dated topic reports; index in [README](../../artifacts/reports/README.md) |
| `artifacts/analysis/` | raw evidence, logs, TSVs, test binaries by dated topic |
| `artifacts/protocol/`, `profiles/`, `ui-assets/` | keymaps, hardware/software profiles, theme assets |
| `openaurora/` | replacement source, host tests, TV builds, RE helper scripts |
| `openaurora/tools/re/` | `DumpFirmware.java`, `FindCallers.java`, `DecompileRaw.java` |

Ghidra project map (details and hashes in [binaries.md](binaries.md)):

| project directory | program | source |
|---|---|---|
| `ghidra_proj` | `aurora.elf` | `/vendor/com.nvt.idtv/aurora.elf` |
| `ghidra_ndal3` | `libNDAL3.so` | `/vendor/lib/libNDAL3.so` |
| `ghidra_ndal_ex` | `libNDAL_EX.so` | `/vendor/lib/libNDAL_EX.so` |
| `ghidra_extin` | `ntkndpextin.ko` | kernel module |
| `ghidra_ntk` | `ntkndpdrm.ko` | kernel module |
| `ghidra_nondovi` | `ntkdriver_nondovi.ko` | `/vendor2/modules/` |
| `ghidra_tdga` | `libdirectfb_tdga.so` | `/vendor/lib/dfb/.../gfxdrivers/` |
| `ghidra_eth` | `ntkethmac.ko` | kernel module |
| `ghidra_preinit` | `PreInit.ko` | kernel module |
| `ghidra_hdmiswitch` | `ntkndphdmiswitch.ko` | kernel module |
| `ghidra_alsa` | `ntkndpalsa.ko` | kernel module |
| `ghidra_mali` | `libmali.so` | partial initial analysis |

## Quick lookup

| I need… | Read |
|---|---|
| Boot chain, `/conf` hook, recovery, watchdog, persistent-start gates | [subsystems#boot](subsystems.md#boot-init-and-recovery); COLD-START-STAGE-AUDIT, BOOT-HOOK-COLD-ATTEMPT, STAGED-COLD-BOOT-DESIGN |
| OSD/fb geometry, DRM/KMS, MAU, scanout, EGL | [subsystems#display](subsystems.md#display-osd-drm-gpu); OSD-BUFFER-CONTRACT, OSD-KERNEL-INIT, OSD-MAU-SELECTION, COLD-DISPLAY-REPAIR, EGL-WINDOW |
| Panel power/backlight/TCON | [subsystems#panel](subsystems.md#panel-tcon); PANEL-BACKLIGHT, PANEL-CURVE-NIM-DTV, PANEL-HAL-ERRORS |
| Audio HAL, routing, volume, AQ, amp, HP/EP, S/PDIF | [subsystems#audio](subsystems.md#audio); AUDIO-ROUTING, AUDIO-VOLUME, AUDIO-AMP-NATIVE, AUDIO-SOURCE-SERVICE |
| CEC transport, ownership, ARC/System Audio, live bus | [subsystems#cec](subsystems.md#cec); CEC-TRANSPORT-CONTRACT, CEC-OWNERSHIP, CEC-LIVE, CEC-SPEC |
| Source/VIN/V4L2/DRM switching | [subsystems#source](subsystems.md#source-vin-v4l2); SOURCE-PATH, SOURCE-VIN, SOURCES-CAPTURE |
| Tuner/NIM-DTV/MXL661/SPI | [subsystems#tuner](subsystems.md#tuner-nim-dtv); TUNER-BRINGUP, TUNER-MXL661, TUNER-VENDOR-CHANNEL |
| GPIO/I2C/SPI pin and bus contracts | [subsystems#gpio](subsystems.md#gpio-i2c-spi); AMP-GPIO-QUERY, AMP-I2C, SPI-MTD-NIM-STAGE, VENDOR-INVENTORY |
| IR keymaps, BLAST/UART, repeat | [subsystems#ir](subsystems.md#ir); IR-KEYMAP, IR-UART, IR-BLAST-HELPER |
| Settings DB, appmgr, launcher items, keyboard | [subsystems#settings](subsystems.md#settings-db-appmgr-launcher); SETTINGS-*, APP-MANAGER-KEYBOARD, APP-MANAGER-CONFIG |
| Vendor UI geometry/assets/fonts/animations, native Omarchy | [subsystems#ui](subsystems.md#ui-vendor-and-native); UI-PARITY, UI-ANIMATIONS, SVG-*, STOCK-APPS-GRID-PARITY, NATIVE-OMARCHY-UI |
| EPG, PVR, channels, broadcast | [subsystems#epg](subsystems.md#epg-pvr-channels); EPG-*, PVR-*, CHANNEL-RUNTIME-ORDER, VENDOR-BROADCAST-ABI |
| Power/standby/wake/network | [subsystems#power](subsystems.md#power-standby); POWER-STANDBY-CONTRACT |
| PQ/MPFW kernel tasks and the load-average artifact | [subsystems#pq](subsystems.md#pq-mpfw-and-kernel-tasks); PQ-MPFW-TASK-2026-11-09 |
| Every binary/ioctl in the vendor tree | [binaries.md](binaries.md); VENDOR-INVENTORY-2026-11-07 |
| How to search/decompile/extract | [tooling.md](tooling.md) |
| Unresolved contracts | [open-questions.md](open-questions.md); FINDINGS/TODO |

## Highest-value reports

| report | why it matters |
|---|---|
| [VENDOR-INVENTORY-2026-11-07](../../artifacts/reports/VENDOR-INVENTORY-2026-11-07.md) | 461 ELF files hashed; 15 platform modules; 208-entry GPIO table; NDAL/VPLTIO ioctl call-site inventories |
| [PLATFORM-PARITY-GAP-2026-11-08](../../artifacts/reports/PLATFORM-PARITY-GAP-2026-11-08.md) | what the replacement still owes, hardware owner by hardware owner |
| [OSD-BUFFER-CONTRACT-2026-10-31](../../artifacts/reports/OSD-BUFFER-CONTRACT-2026-10-31.md) | OSD/fb/MAU/DRM contract, kernel provenance |
| [OSD-MAU-SELECTION-2026-10-31](../../artifacts/reports/OSD-MAU-SELECTION-2026-10-31.md) | MAU ioctl and memory selection |
| [AUDIO-ROUTING-2026-11-01](../../artifacts/reports/AUDIO-ROUTING-2026-11-01.md) | AOUT cache, main amp identification (AD82587_D) |
| [AUDIO-VOLUME-2026-11-01](../../artifacts/reports/AUDIO-VOLUME-2026-11-01.md) | AQ 101-entry table, gain scales |
| [AUDIO-AMP-NATIVE-2026-11-04](../../artifacts/reports/AUDIO-AMP-NATIVE-2026-11-04.md) | amp init/reset/mute native sequences |
| [AUDIO-SOURCE-SERVICE-2026-11-09](../../artifacts/reports/AUDIO-SOURCE-SERVICE-2026-11-09.md) | ICE_AudioStart external branch, AIN/VAUD/AQ/AQuality, HP |
| [CEC-TRANSPORT-CONTRACT-2026-10-31](../../artifacts/reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md) | raw/VPL packet ABI, receive/filter/close hazards |
| [CEC-OWNERSHIP-2026-11-01](../../artifacts/reports/CEC-OWNERSHIP-2026-11-01.md) | PreInit cdev, raw backend, owner lifetime |
| [CEC-LIVE-2026-11-01](../../artifacts/reports/CEC-LIVE-2026-11-01.md) | live shared-owner queries, Steam Deck at LA8/PA1000 |
| [SOURCE-PATH-2026-11-09](../../artifacts/reports/SOURCE-PATH-2026-11-09.md) | VIN/source connect path and HAL seam |
| [TUNER-BRINGUP-2026-11-06](../../artifacts/reports/TUNER-BRINGUP-2026-11-06.md) | NIM/DTV bring-up and gates |
| [APP-MANAGER-KEYBOARD-2026-11-07](../../artifacts/reports/APP-MANAGER-KEYBOARD-2026-11-07.md) | libappmgr symbols, keyboard layout |
| [UI-ANIMATIONS-2026-10-31](../../artifacts/reports/UI-ANIMATIONS-2026-10-31.md) | 39 animation descriptors, 702 resource names/CRCs |
| [UI-PARITY-2026-10-31](../../artifacts/reports/UI-PARITY-2026-10-31.md) | vendor portal geometry/palette and capture provenance |
| [STOCK-APPS-GRID-PARITY-2026-11-05](../../artifacts/reports/STOCK-APPS-GRID-PARITY-2026-11-05.md) | apps grid, focus/highlight state |
| [COLD-START-STAGE-AUDIT-2026-11-06](../../artifacts/reports/COLD-START-STAGE-AUDIT-2026-11-06.md) | recovered `ICE_HardwareInit` stage order |
| [STAGED-COLD-BOOT-DESIGN-2026-11-09](../../artifacts/reports/STAGED-COLD-BOOT-DESIGN-2026-11-09.md) | current cold-boot ladder, watchdog/gate safety |
| [PQ-MPFW-TASK-2026-11-09](../../artifacts/reports/PQ-MPFW-TASK-2026-11-09.md) | why the TV load average is ~15 (D-state msleep pollers) |

## Conventions used in the reports

- Addresses are virtual addresses in the shipped ELF/module unless marked
  “section-relative” (`.ko` objects use section-relative symbols; do not apply
  `.text` offsets to `.init.text`).
- `aurora.elf` is stripped in the ordinary sense but keeps dynamic names: use
  `nm -D`, not plain `nm`.
- External relocation placeholders are not resolved call targets.
- `Thumb`/`ARM` state matters; `BLX` to an even PLT slot enters ARM.
- Vendor `void` returns are common; a successful ioctl/return code does not
  prove the hardware effect.
