# Vendor binary and library inventory — 2026-11-07

Status: read-only extraction and static analysis. No TV execution, no hardware
writes and no vendor binary redistribution. Raw dumps stay under
`artifacts/analysis/vendor-inventory-2026-11-07/`; vendor ELFs remain in the
ignored `artifacts/workbench/rootfs` tree. Full per-file strings and symbol
tables, including aurora's full string dump, are kept local-only under
`artifacts/workbench/vendor-inventory-raw/` because the vendor blobs contain
certificate/key material; only curated maps, hashes and TV-platform analysis
are committed.

## Scope

Source is the extracted `/vendor` tree from this TV's NAND dump
(`artifacts/workbench/rootfs/vendor`). The census found **389 ELF files**,
**249,616,804 bytes**, all **ELF32 / ARM**:

| ELF type | files |
|---|---:|
| shared object (`DYN`) | 331 |
| executable (`EXEC`) | 32 |
| relocatable kernel module (`REL`) | 26 |

Every file has its size, MD5, SHA-256, class/type/machine, SONAME and NEEDED
count in `elf/elf-census.tsv`. The inventory includes third-party browser,
Opera, Amazon, Netflix, Samba and crypto libraries; their symbols and strings
are captured but only the TV platform pieces were decompiled deeply.

Largest files:

```text
64,294,748  vendor/lib/opera/libomi.so
29,259,744  vendor/com.nvt.idtv/aurora.elf
11,729,220  vendor/com.nvt.amazon/install/nova-2019-72671/bin/ignition
 9,759,488  vendor/com.nvt.nflx/netflix
 8,605,776  vendor/lib/libmali.so
 8,177,036  vendor/lib/opera/libavcodec.so.58.18.100
 5,782,340  vendor/lib/libWasabi.so
 5,757,584  vendor/com.nvt.amazon/install/nova-2019-72671/bin/libmozjs45.so
```

## Artifact map

All compressed text is xz; read with `xz -dc FILE`.

| path | contents |
|---|---|
| `elf/elf-census.tsv` | 389 rows: path, size, MD5, SHA-256, ELF class/type/machine, SONAME, NEEDED count |
| `elf/elf-files.txt` | exact ELF path list |
| `elf/dynamic-tags.txt.xz` | `readelf -dW` for every ELF (NEEDED, SONAME, RPATH/RUNPATH, flags) |
| `elf/sections.txt.xz` | `readelf -SW` for every ELF |
| `elf/dyn-symbols.txt.xz` | `nm -D` defined and undefined symbols per ELF |
| `elf/dyn-symbol-index.tsv.xz` | flat 348,443-row symbol index: library, address, type, name |
| `elf/build-metadata.txt.xz` | GNU build-id, `.comment` compiler strings and ARM attributes per ELF |
| `exports/libNDAL3.tsv`, `libNDAL_EX.tsv`, `libappmgr.tsv` | sorted defined dynamic exports (address, type, name) |
| `exports/libappmgr-all-symbols.txt` | 2,476 unstripped appmgr symbols with sizes |
| `keyboard-layout-us.tsv` | 127-slot appmgr US keyboard layout (119 populated) |
| `strings/appmgr-strings.txt.xz` | appmgr strings with file offsets |
| `../workbench/vendor-inventory-raw/elf/symbol-tables.txt.xz` | local-only full `readelf -sW` (ignored; not committed) |
| `../workbench/vendor-inventory-raw/elf/strings.txt.xz` | local-only `strings -a -n 5`; contains vendor certificate/key material, never redistribute |
| `ghidra-summary.tsv` | per-project function/symbol/call-edge/data counts and MD5 |
| `ghidra/<tag>/` | per-program `inventory.txt`, `functions.tsv.xz`, `callgraph.tsv.xz`, `symbols.named.tsv.xz`, `data.tsv.xz` |
| `gpio-function-names.tsv` | full GPIO function-name table (226 slots, 224 named; ID → name) |
| `calls/vpltio-command-map.tsv` | VPLTIO command → calls/callers/sites (45/45 call sites, 39 unique values) |
| `calls/vpltio-sendcmd-r0-objdump.tsv` | raw per-call rows with setup instructions |
| `calls/api-args/` | per-symbol/register call-site maps (GPIO, IIC, audio, DRM, NDAL, PWM, panel) |
| `calls/api-args/ioctl-r1-map.tsv` | aurora `ioctl` command immediates (91 direct sites, 83 immediate) |
| `calls/aurora-plt-call-counts.txt` | call count for every `@plt` import in aurora |
| `strings/aurora-device-paths.tsv` | 288 defined path strings / 690 references with function owner |
| `strings/aurora-subsystems.tsv` | 9,646 subsystem-keyword strings / 4,776 references |
| `../workbench/vendor-inventory-raw/aurora-strings.txt.xz` | local-only 376,113 aurora strings with file offsets; contains embedded key/cert strings, never commit/redistribute |
| `grok-findings.md` | second-opinion NDAL/kernel-module extraction (when present) |

Durable scripts added for reproducing the inventory:

```text
openaurora/tools/re/ProgramInventory.java     Ghidra functions/symbols/callgraph/data
openaurora/tools/re/DumpCallArgs.java         Ghidra immediate-argument call-site scan
openaurora/tools/re/objdump-callsite-args.py  same over the objdump cache
openaurora/tools/re/group-callsite-args.py    aggregate command maps
openaurora/tools/re/DumpStringRefs.java        string -> referencing function
```

## Ghidra inventory

Each project was opened read-only with `-readOnly -noanalysis`. Counts are
Ghidra analysis output, not a claim that every function was identified. The
`aurora` row includes imported/PLT function objects in `functions_total`; the
`functions_named` count excludes analysis-only names.

| tag | program | MD5 | functions | call edges | symbols | data rows |
|---|---|---|---:|---:|---:|---:|
| aurora | aurora.elf | `64c25234...` | 89,220 | 485,969 | 759,749 | 332,334 |
| ndal3 | libNDAL3.so | `c842bad8...` | 1,101 | 5,275 | 12,784 | 9,748 |
| ndal_ex | libNDAL_EX.so | `e6a46586...` | 676 | 2,299 | 6,011 | 4,766 |
| nondovi | ntkdriver_nondovi.ko | `fded7005...` | 14,940 | 168,673 | 244,048 | 131,945 |
| mali | libmali.so | `d1aa5c16...` | 21,230 | 141,096 | 195,460 | 42,749 |
| ntk | ntkndpdrm.ko | `a70aca9b...` | 837 | 2,086 | 9,028 | 8,690 |
| extin | ntkndpextin.ko | `cb0689e9...` | 194 | 386 | 1,655 | 1,839 |
| tdga | libdirectfb_tdga.so | `a770a7e5...` | 214 | 449 | 1,908 | 1,234 |
| alsa | ntkndpalsa.ko | `8758a13d...` | 982 | 6,876 | 9,959 | 7,921 |
| preinit | PreInit.ko | `1e50bee8...` | 716 | 7,111 | 9,252 | 5,705 |
| eth | ntkethmac.ko | `2efe1ee1...` | 334 | 1,082 | 1,259 | 735 |
| hdmiswitch | ntkndphdmiswitch.ko | `d4ff737d...` | 39 | 42 | 269 | 306 |
| ir | libdirectfb_nvt_uart_ir.so | `be21722f...` | 79 | 115 | 421 | 342 |
| appmgr | libappmgr.so | `413930d9...` | 1,556 | 4,553 | 9,718 | 7,634 |
| uboot | uboot-code.bin | `d556db31...` | 1,534 | 8,376 | 17,682 | 4,988 |

`ndalex` is a second Ghidra project for the same `libNDAL_EX.so` MD5; it is a
duplicate analysis, not a different library. `dfb` is likewise the same
`libdirectfb_tdga.so` as `tdga`.

## Recovered VPLTIO command map

`Hal_Video_Vpltio_SendCmd` has 45 call sites in aurora. All 45 immediate `r0`
commands were recovered from the instruction stream; 41 are unique values.
Full evidence with setup instructions is in
`calls/vpltio-command-map.tsv`.

```text
0x152  OverDrive table / SetOverDrive
0x18c  VLIB message enable / SetWindowBGcolor
0x18d  frame mute
0x191  graphic blending
0x1c7  SSC
0x1c8  LVDS clock/data delay
0x1c9  panel LVDS driving / LVDS strength
0x1ca  panel LVDS pre-emphasis
0x1cc  LVDS mute (panel power sequence)
0x1cd  LVDS format
0x1ce  10-bit dither
0x283  VIN MHL connected
0x284  VIN HDMI ARC
0x285,0x287  VIN HDMI PHY
0x2c1  VPLTIO channel scan
0x2c2  VIN VD channel up/down init
0x302  plat_common_init DVIF source
0x303  plat_common_set_video_path_config
0x346..0x349  PC position
0x3c4  auto fine tune
0x44a,0x44b  Macrovision start/stop
0x44e  VBI data
0x481,0x482  VIN PC mode
0x4c1  TV-out enable
0x4c3  double frame rate
0x4c4  dimming
0x4c8  TCON on/off status
0x503  photo LBM format
0x603  TV-out enable (second path)
0x60b  Vsync offset
0x641..0x643  SSC / HDCP key ready
```

This map is static call-site data; it does not prove which commands the panel
accepts or what each argument struct means.

## Aurora ioctl command immediates

`ioctl` has 91 direct call sites in aurora; 83 carry an immediate command in
`r1`. Full rows and setup instructions are in
`calls/api-args/ioctl-r1-map.tsv`. Grouped examples:

```text
0x1260            filesystem drive size
0x541b,0x5421     socket available / socket ioctl
0x6480            GPU DRM copy/bitblit/fillrect/scalerect
0x8912..0x8927    network interface MAC/address/DNS
0x89f0            Ethernet PHY power
0x8b15..0x8bf1    WiFi MAC/scan/connect/link status
0x9200            CEM change state
0x40fc9202        CEM set IDTV state
0x80fc9203        CEM get IDTV state
0x40084d02        MTD erase
0x80044d07        MTD show info
0x80204d01        MTD show info (alternate direction)
0x4018531f        SPI write
0x40185320        SPI erase
0x8018531e        SPI read
0x80185321        SPI read status
0x4024700a        set current time
0x80247009        set current time (alternate direction)
0xc010bd0c        AV encoder encrypt buffer
0xc0185647        DRM DP source
0xc02c6907        IIC get description
0xc088671f        GPIO get description
0xbb44,0xbb54     PVR buffer query / read address
```

The remaining 8 call sites use a non-immediate `r1` (register/memory), so they
are deliberately left UNKNOWN rather than guessed.

## GPIO function-name table

`au8GPIOFuncName` is at **0x175f350** (Ghidra label; file offset
`0x173f350`), 24 bytes per record. The table was extracted from the ELF and
contains **226 slots, 224 named entries**. `astPwmBasePin` is at **0x175f320**.

```text
0   TUNER_IIC_SDA            108 BACKLIGHT_LEVEL
1   TUNER_IIC_SCL            110 PANELPWR
4   LNB_FAULT                111 BKLTPWR
5   LNB_EN                   189 LED_R
10  IISOUTA_DATA             190 LED_G
...
```

The full list is `gpio-function-names.tsv`. This table is the authoritative
ID → name mapping used by `DEV_GPIO_GetDescription`; it corrects the earlier
live probe's mislabelled `id`/`pin` columns (the ioctl's word 0 is the pin,
word 1 is the function id, and `DEV_GPIO_GetDescription` returns `entry+8`).

## Audio control opcodes

`Hal_Audio_Ctrl` has 59 `@plt` references; 58 direct call sites were matched and all 58 carry an immediate `r0` opcode (37 unique).
The complete map is `calls/api-args/Hal_Audio_Ctrl-r0-map.tsv`. Platform-facing
examples:

```text
0x15 speaker data type      0x2c/0x2d speaker set
0x16 speaker balance        0x2e volume table config
0x1c downmix mode           0x33 main volume
0x1d AC4 dialogue gain      0x35 speaker HW mute
0x1e VAUD/AIN init          0x3a I2S MCLK (amp init)
0x22 audio sync master      0x3c/0x3d speaker main/mix gain
0x2a AIN connect            0x3e volume table index
```

This is a call-site map only; the 56-byte parameter block semantics per opcode
remain defined by the contract reports and instruction checks.

## Device-path references

`DumpStringRefs.java` produced 288 path strings and 690 references
(`strings/aurora-device-paths.tsv`). Confirmed in aurora itself:

```text
/dev/ntgpio      DEV_GPIO_Init
/dev/ntkiic0     DEV_IIC_Init
/dev/ntkspi      DEV_SPI_Open
/dev/ntkfb0      plat_Gfx / MCU semaphore
/dev/nvt/ntkfb0  plat_Gfx / MCU semaphore
/dev/ntksys      plat_Gfx_DRM_osd_drv::ShareMem_Create
/dev/fb0,fb6     MCU semaphore
/tmp/ntkdriver_completed  ICE_HardwareInit
```

`/dev/ntkled`, `/dev/ntkcec` and the NDAL V4L2 device nodes are opened inside
the vendor libraries/kernel modules, not via aurora string literals; they are
covered by the NDAL/kernel inventory instead of this table. The 9,646-string
subsystem table (`strings/aurora-subsystems.tsv`) maps CEC/HDMI/tuner/panel/
audio/GPIO/PWM/STBC/DRM/OSD strings to their owning functions.

## Panel/display constants confirmed in the binary

| symbol | address | note |
|---|---:|---|
| `genPwmdimMode` | 0x1761870 | stock value 0 |
| `gu16PanelHighPeriodIn1000` | 0x1761876 | cached backlight level |
| `gu32PanelFreqInHz` | 0x1761878 | stock value 30000 |
| `gstBacklightPara` | 0x19354c4 | level + runtime minimum |
| `gstVoutPwmParams` | 0x19354cc | curve thresholds/midpoints |
| `astPwmBasePin` | 0x175f320 | pin 108 → channel 0 |
| `au8GPIOFuncName` | 0x175f350 | 226-slot (224 named) table |

`DEV_PANEL_SetBacklightLevel` receives `0x4000` at `0x7e3c96`
(`ice_BacklightSetPanelBright`); that is outside the documented 0..1000 range
and remains an unexplained vendor special value. Do not copy it without
checking the caller/arguments.

`NDAL_VideoExt_OpenDevice(0, 10)` is the only direct immediate pair in aurora
(callers `DEV_Ipc_init`, `PLAT_DRV_INI_Init`, `PLAT_VIU_Init`) and matches the
video24 global fd evidence. `Hal_Video_DrmInit(1)` has one immediate call site
in `plat_common_init` at `0x9248f2`.

## High-value libraries not yet decompiled deeply

The census and dynamic-symbol index cover every ELF, but Ghidra projects exist
only for the TV platform pieces. Libraries worth a future deep pass, in rough
priority order for the replacement:

```text
libdirectfb-1.7.so.1    core DirectFB below the analyzed TDGA OSD layer
libSDP_OMX_Core.so      OpenMAX/media pipeline
libesplyrclient/srv.so  media player IPC
libnbinder.so           binder-style IPC used by framework services
libWasabi.so            PlayReady/DRM (not a replacement target; do not port)
libkeyutil/libcrypt_ntk DRM/key handling (not a replacement target)
```

`libomi.so`, `libavcodec/format`, Amazon/Netflix and Opera libraries are
third-party application payloads; they are inventoried but deliberately not
decompiled.

## Unstripped app-manager recovery

`libappmgr.so` is **not stripped** (MD5 `413930d9...`, BuildID
`fd36d841...`): 2,476 symbols with sizes, 693 defined dynamic exports, 1,556
functions and 4,553 resolved call edges in the new Ghidra project. Recovered:

- full US keyboard layout, 127 slots × 48 bytes at `keyboard_layout` 0x52f50,
  119 populated keys, including DirectFB navigation/F-key codes
  (`keyboard-layout-us.tsv`);
- `KBL_mapExtKeycode` 0x33651 and `KBL_findCode` 0x33721, with the entry/
  subrecord structure;
- HID/IR path: `HID_CommandIrInput` 0x30f74, `COM_SendKey` 0x3678c,
  `HID_NamKeyToDfbKey` 0x32c14, `HID_DfbKeyToNamKey` 0x32b98,
  `HID_HandleWindowEvent` 0x313c0, `HID_SendWinEventToApp` 0x31c1c and
  `MGR_SendCommand` 0x2a550.

Details and the key table are in APP-MANAGER-KEYBOARD-2026-11-07. The table has
not been executed through the library; `keycode_ext_layout` and
`HID_LoadKeycodeExtMap` remain undecoded.

## Corrections and limits

- `DEV_PANEL_PowerSequence` is a standby/power-off/reset path, not a boot
  initializer; see PANEL-BACKLIGHT-2026-11-07. Boot panel power is earlier
  (xboot/kernel/PreInit).
- The panel backlight HAL expects the 0..1000 domain after
  `plat_set_dimming`; passing 0..4095 saturated 50%/100% and explained the
  no-dimming hardware negative. The module now converts at the boundary.
- The GPIO live probe's printed `id`/`pin` labels were swapped; the table and
  `DEV_GPIO_GetDescription` pointer arithmetic above are the corrected reading.
- Ghidra function/call counts are analysis-specific; imported/thunk function
  objects and unresolved indirect calls are not hardware behavior.
- The callgraph contains only resolved direct call flows. VPLTIO/command maps
  contain only call sites with a local immediate; indirect/dynamic values are
  omitted rather than guessed.
- No TV operation, driver call, scanout or audible test was performed for this
  inventory.

## Reproduction

```sh
# per-program Ghidra inventory (read-only)
/opt/ghidra/support/analyzeHeadless artifacts/workbench/ghidra_proj Aurora \
  -process aurora.elf -readOnly -noanalysis -scriptPath openaurora/tools/re \
  -postScript ProgramInventory.java <outdir>

# immediate call-site map from the objdump cache
python3 openaurora/tools/re/objdump-callsite-args.py \
  artifacts/workbench/aurora.dis Hal_Video_Vpltio_SendCmd r0
python3 openaurora/tools/re/group-callsite-args.py <tsv> <map.tsv>
```

Source hashes are in `elf/elf-census.tsv`; the aurora MD5 is
`64c252340d3af6c5d66072611b7bad7a`, `libNDAL3.so`
`c842bad804302ec822076c3d9d3ce31d`, `libNDAL_EX.so`
`e6a465861d1869b8b138b3f299c89f87`. Aurora's GNU build-id is
`4a16c2b5be5987c079a744b7323621d5ee0fd243`; its ARM attributes are
`Tag_CPU_name 7-A`, `Tag_CPU_arch v7`, `Tag_ABI_VFP_args VFP registers`.
