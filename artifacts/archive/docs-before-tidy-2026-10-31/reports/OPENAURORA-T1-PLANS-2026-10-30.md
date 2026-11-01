# openaurora — T1 module plans (Ghidra-derived) — 2026-10-30

Planning pass done while the set is unavailable. Everything below is grounded in the binary, not in
assumption. Two findings reorder the work; they are at the top because they change the cost of every
module.

## 0. What is actually reusable

`libNDAL3.so` exports **only `Hal_*` — 643 functions in 18 families**. `libNDAL_EX.so` exports
`NDAL_*` (160). **Nothing** exports `PLAT_*`, `DEV_*` or `ICE_*`: those are statically linked into
`aurora.elf` and cannot be called from outside it.

| family | fns | what it covers | module that uses it |
|---|---:|---|---|
| `Hal_Video` | 124 | planes, DRM helpers (`Hal_Video_Libdrm_drmModeSetPlane`, `drmModeRmFB`, `Hal_Video_Drm_Set_Property`) | `display` |
| `Hal_Demux` | 116 | DVB demux, CI status/encryption, STC | tuner (T2) |
| `Hal_OS` | 108 | mutex, semaphore, IPC | all |
| `Hal_PQ` | 97 | HDR10+, xvYCC, black level, picture modes | `pq` (T2) |
| `Hal_Audio` | 60 | audio control, mixer, protection, IPC command channel | `audio` |
| `Hal_VideoDec` | 26 | v4l2 decoder plumbing | `apps`/media |
| `Hal_Vin` | 22 | **video input**: `Hal_Vin_Close`, `Hal_Vin_SetPcMode`, `Hal_Vin_SetStreamStart`, … | **`source`** |
| `Hal_Gpio` | 20 | GPIO read/write/config/pinctrl | `panel`, `power` |
| `Hal_Pwm` | 12 | backlight PWM | `panel` ✓ |
| `Hal_SPI` | 12 | SPI flash | firmware tooling |
| `Hal_Iic` | 6 | I²C read/write (amp, tuner) | `audio`, tuner |
| `Hal_Menu` | 6 | OSD/menu hooks | `display` |
| `Hal_Userdata` | 6 | VBI/user-data lines | teletext (T2) |

Consequence: `display` and `source` are **not** from-scratch work — the vendor ships the DRM wrappers
and the video-input API we would otherwise have to reverse-engineer from ioctls.

## 1. `power` — suspend/resume

Fully recovered from `ice_hardware_SuspendToRam` (`0x752f70`), in call order:

```
ice_external_SetCISource(1)                 # park CI before sleeping
ice_IrDisable(1)                            # stop the IR receiver
[if Profile_IsBootLogoEnabled] ICE_RemoteEnableReception(0)
NonVolatile_SystemShutdown()                # flush NVRAM *before* sleep
_ice_lowpower_convert_time(&t, &timeout, 1) # timeout encoding
PLAT_SYS_SetMcuWakeUpTime(&t)               # MCU keeps a wake timer
sync()
fd = open("/sys/power/state", O_WRONLY)
PLAT_SYS_MCPU_MonitorTask_Reset(1)
ice_ci_resetCiReady(); idtvSuspend()
write(fd, "mem", 3)                         # <-- the actual suspend
ice_video_ResumeStartTime(); idtvResume()
PLAT_SYS_MCPU_MonitorTask_Reset(0)
NonVolatile_Initialize()                    # NVRAM back
DEV_Uart_ResumeClean()                      # UART/IR clean
ice_IrDisable(0)
ice_SendKeyToMw(0x21f)                      # tell middleware we are back
```

**Plan.** `mod_power` owns standby:
1. `suspend`: emit `power/state = STANDBY`, `suspend` every module in reverse order (our `ir` closes
   the tty, `panel` closes PWM), `sync()`, `open("/sys/power/state")`, `write("mem", 3)`.
2. On return (write unblocks after resume): re-init NVRAM equivalent (we have none yet), resume
   modules in order, emit `power/state = ON`.
3. Publish a `power` service so a UI can request standby without knowing the mechanism.

**Reusable:** `/sys/power/state` is plain Linux. `PLAT_SYS_SetMcuWakeUpTime` is aurora-internal; the
MCU wake timer is probably optional for our use (wake by IR/WoL), but must be tested.

**Unknowns:** which state string the set accepts (`mem` is what aurora writes; `freeze`/`standby` are
alternatives), the `_ice_lowpower_convert_time` struct, and whether `idtvSuspend/Resume` are needed
for the MCU (they are aurora's own video/DTV hooks, not hardware). Also the quick-vs-deep selection
we saw referenced (`isSuspendToRamEnabled`, `getStartInQuickStandby`, OLED flag).

**Test.** As root with our stack stopped: `echo mem > /sys/power/state`, wake with the remote, then
with WoL; then repeat through `mod_power`. Measure: does the MCU wake timer need setting at all?

## 2. `source` — input switching

Recovered surface:
* `DEV_SRC_*` (aurora-internal, 11 fns): `ConfigFrontEndDevice(_level)`, `HDMIHotPlugCtrl`,
  `HDMIHotPlugAfterResume`, `SetHdmiHotplug`, `CheckSrcSupportStatus`, `GetARCPortPhyAddr`,
  `GetMHL_HDMIPort`, SCART pin-8 detection, IR-sync.
* `ICE_External*` (aurora-internal): `NumberOfInterfaces`, `GetName`, `GetVideoType`,
  `GetCapability`, `Init`, `Open`.
* `ExternalAVSourceManager::selectExtSource(idx, secondary)` — routes **video and audio separately**,
  then publishes state.

**Plan.** `mod_source` publishes a `source` service:
* list = the 13 ICE descriptors filtered by `SRC_CFG_SourceSupportTable` (enabled `0,1,2,3,4,10,14,15,16,19,20`);
* switch = `Hal_Vin_*` for the video/audio input path (`Hal_Vin_SetStreamStart`,
  `Hal_Vin_SetPcMode`, `Hal_Vin_Close`), plus `Hal_Audio_*` for audio routing;
* hotplug = `Hal_Vin` + `Hal_Gpio` for HPD if needed.

**Unknowns:** the `Hal_Vin` argument ABI (channel numbering vs our PA→group table `{0x1000→3,
0x2000→4, 0x3000→9, 0x4000→12}`); whether HDMI RX lives behind `Hal_Vin` or the `ntkndphdmi.ko`
ioctl; how `selectExtSource`'s "secondary" (PIP) path maps.

### `Hal_Vin` ABI (decompiled)

Only 11 functions, and the switching path is small:

```c
void Hal_Vin_Open(int idx);                  // opens /dev/ntkdvif (per-index suffix), returns fd
void Hal_Vin_SelectInput(int fd, int input); // ioctl(fd, 0xc0045627, &input)
void Hal_Vin_SetStreamStart(int fd);         // ioctl(fd, 0x40045612, &{1})
void Hal_Vin_GetHdmiStatus(int fd, void *out);
void Hal_Vin_GetRes / SetInputRes / SetPcMode / SetColorSystem / GetLock / GetVDLock
```

All `void`; errors only surface as `perror` inside the HAL. The port is `/dev/ntkdvif` (there is also
`/tmp/ntkdvif`, created if the global device node is missing).

aurora's entry point is `plat_vin_slot_connect(slot, source_type)`: for source types `0, 4, 5, 0x14,
0x15` it does `Hal_Vin_Open(0)` → `FUN_0092e8ac(fd, type)` (which calls `Hal_Vin_SelectInput`) →
`Hal_Vin_SetStreamStart(fd)` → `Hal_Video_Set_V4l2Info(...)`. So the HDMI/AV inputs all come up as a
**V4L2 device bound to a VIN slot**, which is why `Hal_Vin` is the right API rather than the DRM layer.

**Still needed before `mod_source` can switch inputs:** the numeric `input` id passed to
`Hal_Vin_SelectInput` (inside `FUN_0092e8ac`) and the mapping from source type → id. That is one more
focused decompile; the module itself will then be small (open fd, select, start stream, publish a
`source` service). Enumerating inputs is already known from the ICE tables.

### The id table (read from .data, resolved)

`FUN_0092e8ac(fd, source_type)` looks the id up in a 26-entry table at `0x013b9e78`, sends a VPLT
command with a companion table at `0x013b9ee0`, then calls `Hal_Vin_SelectInput(fd, id)`:

| source type | SelectInput id | | source type | SelectInput id |
|---:|---:|---|---:|---:|
| 0 | 1 | | 16 | 2 |
| 5 | 0 | | 17 | 3 |
| 10 | 2 | | 18 | 3 |
| 15 | 1 | | 19 | 66 |
| | | | 20 | 3 |
| | | | 21 | 3 |
| | | | 25 | 255 |

(types not listed map to 0). So the whole switching path is: `Hal_Vin_Open(0)` →
`Hal_Vin_SelectInput(fd, table[type])` → `Hal_Vin_SetStreamStart(fd)` → `Hal_Video_Set_V4l2Info(...)`.
That is all `mod_source` needs, plus the input list itself (already known from `ICE_External*` and
`SRC_CFG_SourceSupportTable`).

**Test.** Without touching the panel: select input and read back `Hal_Vin` state; verify with the
existing DIAL `KeyAux` path as the oracle.

## 3. `display` — planes and framebuffer

Two layers, both usable:
* kernel side: `ntkndpdrm.ko`, standard DRM (`drm_gem_cma`, planes, dumb buffers) — `drmprobe` is
  written and pending;
* vendor side: `Hal_Video` (124) exposes **`Hal_Video_Libdrm_drmModeSetPlane`**,
  `Hal_Video_Libdrm_drmModeRmFB`, `Hal_Video_Drm_Set_Property`, i.e. the wrappers aurora itself uses.
  `libdirectfb_tdga.so` shows the legacy path (`0xc8` ioctls) if we ever need it.

**Plan.** `mod_display` takes KMS, creates a dumb buffer, and offers a `display` service
(`present(fb_id)`, `plane_info()`). Prefer `Hal_Video_Libdrm_*` over hand-rolled ioctls where it
adds value (property setting, plane commit), keep plain DRM for dumb-buffer creation.

**Unknowns:** z-order/alpha property names, whether fb0..4 map 1:1 to planes, and whether the
kernel hands us a mode at boot with aurora absent — all answered by the first `drmprobe` run.

## 4. `audio` — 60 `Hal_Audio_*` functions

Known entry points:
* `DEV_AMP_Init`: `Hal_Audio_Ctrl(0x3a /*HAL_AUDIO_SET_IISMCLK*/, {0x100})`, then a task
  `"DEV_AMP_ID"`;
* `DEV_AMP_*` are **vtable dispatchers** into a chip object from `DEV_AMP_FACT_Create`; the chip is
  an **AD82586C** (I²C, RAM/register dumps, EQ, subwoofer);
* `PLAT_AIN_*` also calls `Hal_Audio_Ctrl` for input gain/connect;
* `Hal_Audio_Cmd_*` is an IPC command channel (`SockFd_Get`, `Read_Key`, `Send_Msg`) — the audio HAL
  runs partly as a server.

**Plan.** `mod_audio` publishes `audio` (volume/mute/ARC): initialise via
`Hal_Audio_Proxy_Init` → `Hal_Audio_Mixer_Ctrl_Init` → `Hal_Audio_Protect_Init`
(the order aurora's `PLAT_DRV_INI_Init` uses), then volume via `Hal_Audio_SetControl/GetControl`.

**Next RE step (cheap, offline):** extract the **numeric opcode** passed to `Hal_Audio_Ctrl` at every
call site (`0x3a` seen in `DEV_AMP_Init`). That opcode table is the audio API, and it is a finite,
mechanical list — 57 call sites.

### The `Hal_Audio_Ctrl` opcode table (extracted: 37 opcodes, 56 call sites)

`Hal_Audio_Ctrl(opcode, param_struct)` is the whole audio API. Each opcode below was read from the
first argument at every call site in aurora, paired with the function that uses it:

| opcode | meaning (from the calling function) | used by |
|---|---|---|
| `0x11`, `0x1e`, `0x40` | audio-input init steps | `PLAT_AIN_Init`, `PLAT_VAUD_Init` |
| `0x12`, `0x13` | SIF deviation / SIF control | `PLAT_AIN_*` |
| `0x16`, `0x37` | speaker balance | `PLAT_AOUT_SetSpeakerBalance`, `PLAT_VAUD_Init` |
| `0x19` | AAC function | `PLAT_DEC_SetAudioAACFuntion` |
| `0x1b`, `0x1f`, `0x20` | audio-description decode / language / volume | `PLAT_DEC_*` |
| `0x1c`, `0x24` | downmix mode, pan/fade | `PLAT_DEC_*` |
| `0x1d` | AC4 dialogue enhancement gain | `PLAT_AOUT_*` |
| `0x22` | A/V sync master | `PLAT_DEC_SetAudSyncMaster`, `PLAT_VAUD_SetSyncMode` |
| `0x23` | Dolby bypass capability | `PLAT_AIN_*` |
| `0x25`, `0x26` | capture mute, DTS DRC | `PLAT_VAUD_*` |
| `0x2a` | audio-input **connect** (routing) | `PLAT_AIN_Connect` |
| `0x2c`, `0x2d` | speaker DAC CIC gain | `PLAT_AOUT_SetSpeakerDACCICGain` |
| `0x2e`, `0x3e` | volume table config / index | `PLAT_AOUT_VolTableConfig` |
| `0x2f`, `0x30` | SPDIF data type, copy protection | `PLAT_VAUD_*`, `PLAT_AOUT_*` |
| `0x32` | input gain | `PLAT_AIN_SetInputGain` |
| **`0x33`** | **main volume** | `PLAT_DEC_SetAudioMainVolume` |
| **`0x35`** | **speaker hardware mute** | `PLAT_AOUT_SetSpeakerHWMute` |
| `0x36` | DAC inverse | `PLAT_VAUD_SetDACInverse` |
| **`0x3a`** | **I2S MCLK / amp clock** | `DEV_AMP_Init` |
| `0x3c`, `0x3d` | speaker main / mix gain | `PLAT_AOUT_SetSpeakerMainGain` |
| `0x41`, `0x46` | unclassified (decoder / amp init paths) | `FUN_00886e9c`, `FUN_00887244` |

So `mod_audio` needs **six** opcodes, not a driver rewrite: init (`0x11`,`0x1e`,`0x40`), routing
(`0x2a`), volume (`0x33`, with the table at `0x2e`/`0x3e`), mute (`0x35`) and amp clock (`0x3a`).

**Unknowns:** the parameter struct layout per opcode (each call passes a differently shaped stack
struct), whether the AD82586C programming sits inside the HAL (likely), and the `ForceAMPInitial`
retry semantics.

## 5. `panel` — done, pending calibration

`Hal_Pwm_Init/Open/Set_ByChannel` implemented and proven to reach the HAL under qemu. Missing: find
the real PWM channel (config `panel.channel` 0..7) and add panel-power via `Hal_Gpio_*`.

## 6. `settings`

`settingsDB.db` has **66 tables** (`Settings`, `LauncherItem`, `ChanDataTable`, `CIPlusTable`,
`AlexaReadyTable`, `AdvertisingID`, `Privacy_*`, `PortalUrlItem`, …). Plus:
`UserPreferences` (821 fns), `ProfileConfiguration` (461), `Profile` (692), `NvramStorage` (94).

**Plan.** `mod_settings` owns the DB (read/write) and a `settings` service with typed getters. Cache
in memory; write back on change. NVRAM (the MCU-backed blob) is separate and only needed for
power/panel bring-up — defer.

**Unknowns:** which tables are authoritative vs cache, and whether writes need a reboot (we already
know `LauncherItem` does).

## 7. `services`

DIAL 56789/56790, state WS 7681, DLNA 2870 — all already proven standalone (we run `dialserver` from
the hook). `mod_services` would supervise them; no new RE needed.

## 8. Order of work, with gates

| step | module | gate before starting |
|---|---|---|
| 1 | `power` | **nothing** — pure Linux + we can test `echo mem` ourselves |
| 2 | `audio` | opcode extraction (offline) |
| 3 | `source` | `Hal_Vin` ABI (offline decompile) |
| 4 | `display` | `drmprobe` on the set |
| 5 | `settings` | none (offline) |
| 6 | `services` | none |

`power` first is deliberate: it is the one T1 module with no RE left, and it unlocks unattended
testing of everything else (reboot/wake cycles without a human).

## 9. Measurements wanted on the set, in priority order

1. `drmprobe` read-only → driver name, planes, formats; then `--paint` (blocks `display`).
2. `echo mem > /sys/power/state` with our stack stopped → resume behaviour, IR wake, wake timer
   (blocks `power`).
3. `ls /dev/nvt/` (`ntkled`, `ntksys`, …) and `cat /proc/devices` → which HAL families are usable
   (`panel`, `audio`, `source`).
4. `panel.channel` sweep 0..7 → backlight responds.
5. `irkey` → confirm the 52-button map.

## 10. Risks

* **`Hal_*` init order is undocumented.** `Hal_Pwm` needed Init→Open→Set; audio likely needs
  Proxy→Mixer→Protect. Getting it wrong may crash rather than return an error.
* **Two libcs, one process**: our binary must stay glibc; never load these libs from a musl helper.
* **Firmware coupling**: signatures are for `0.51.52.3`. `dlsym`-less linking means an OTA would be a
  hard break — pin the firmware.
* **No isolation**: a HAL segfault takes the whole UI down. If that becomes a problem, split the
  riskiest HAL users into a helper process.
