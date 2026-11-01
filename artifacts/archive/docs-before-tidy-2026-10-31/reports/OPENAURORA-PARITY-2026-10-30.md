# openaurora parity — what aurora actually contains (Ghidra census) — 2026-10-30

Cross-check of the planned module decomposition against the real binary, so "perfect replacement"
is defined by evidence rather than by memory of what the TV seems to do.

Method: Ghidra headless over the analysed `aurora.elf` project (`Aurora`), script
`/tmp/opencode/ghidra_scripts/AuroraCensus.java`, output `/tmp/opencode/aurora_census.txt`:

1. every defined function grouped by namespace/class (84,547 functions total),
2. every call into the `PLAT_*` / `ICE_*` / `DEV_*` / `NDAL_*` / `NEBULA_*` hardware boundary,
3. a keyword census over defined strings.

Caveat: symbol grouping is a measure of *code volume*, not of importance. A feature with 4,000
strings is not four times as necessary as one with 1,000 — but the zeros are meaningful.

## 1. The big function groups

| functions | group | what it is | our module |
|---:|---|---|---|
| 1,568 + 1,113 + 276 | `EASIControlTask` / `EASIControlInterface` / `EASITVControlInterface` | the engine: source routing, audio/video binding, tuner control | `source`, `audio`, `tuner` |
| 1,460 + 159 | `ICE*` / `ice` | Novatek hardware abstraction (the `ICE_*` API) | spread across all hardware modules |
| 1,010 + 202 + 92 | `NEBULA*` / `nebula` / `NebulaBroadcastClient` | cloud portal/market client | `portal` (Tier 3) |
| 895 | `DemoApp` | the application object that owns every manager | `core` + `ui` |
| 820 + 692 + 461 + 94 | `UserPreferences` / `Profile` / `ProfileConfiguration` / `NvramStorage` | settings, persistence, NVRAM | `settings` |
| 530 + 102 + 110 + 104 + 91 | `UIFrameworkControlInterface` + dialogs (`LauncherDialog`, `DialogNavigator`, `ThumbnailDialog`, `GlobalSearchDialog`) | the UI framework | `ui` (phase 2) |
| 314 | `NovoLoggingManager` | logging + telemetry | `logging` (policy) |
| 295 + 197 + 204 | `NovoController` / `ServiceSupervisor` / `NovoSystemInitialiser` | lifecycle orchestration | `core` |
| 259 | `NovoTesttool` | factory test tool | not needed |
| 200 + 194 + 112 + 94 | `NovoPictureSettings` / `PLAT_PQ` / `PLAT_FRC` / `PictureSettingsCapability` | picture quality, frame-rate conversion | `pq` |
| 193 + 111 + 184 + 105 + 108 | `TunerManager` / `ServiceFinder` / `ApplicationServiceList` / `CommonServiceList` / `SatelliteFastScanServiceFinder` | tuner + service lists | `tuner` |
| 163 + 115 + 106 + 104 + 112 | `DvrHelper` / `DvrController` / `NovoDvrController` / `NebulaPvrRecordingScheduler` / `FilePlaybackState` | recording, timeshift, playback | `pvr` |
| 180 + 163 + 161 + 149 + 104 + 95 | `CIHost` / `CHIL` / `ConditionalAccessIF` / `CAStackServer` / `CAStackController` / `CIStackProxy` | CI+ / conditional access | `ci` |
| 167 + 139 + 94 | `MHEGClient` / `fvp::FreeviewPlay` / `TeletextIF` | interactive TV, HbbTV/Freeview Play, teletext | `teletext`/`mheg` |
| 180 | `INCA` | CI stack component | `ci` |
| 162 + 127 + 114 + 108 | `OperaEngine` / `OperaBrowserCore` / `WebBrowser` / `OperaEventProcessor` | OMI/Opera browser | `apps` |
| 161 + 115 | `NovoAVSClient` + `alexaClientSDK` | Alexa Voice Service | `voice` (Tier 3) |
| 110 | `BluetoothController` | Bluetooth | `bt` (Tier 2) |
| 100 + 96 | `NovoNetworkSettings` / `IPMultipleDeviceSettingsDialog` | network config UI | `network` |
| 106 | `NovoPortalController` | portal tiles/launcher | `ui`/`portal` |
| 96 | `VirtualRemoteHandler` | DIAL virtual remote | `services` |
| 91 | `NovoHotelModeSettings` | hotel mode | `hotel` (Tier 3) |
| 93 + 97 + 90 | `InputManager` / `VideoIF` / `AudioIF` | input + AV routing | `source`/`audio` |
| 196 | `StandbyController` | standby/resume | `power` |

## 2. The hardware boundary, by call volume

```
DEV_GPIO   175 calls / 15 fns     GPIO, panel power, amp enable
DEV_NIM    116 / 76               tuner demod (incl. SI2151)
DEV_AMP     82 / 10               amplifier init/mute/HP
DEV_PANEL   57 / 13               backlight PWM, panel power
DEV_SPI     24 / 9  DEV_IIC 22/4  board busses
DEV_TCON    16 / 10               timing controller (panel sequencing)
DEV_UART    12 / 8   DEV_Uart 11/7  the MCU UART (our `ir` module)
DEV_MTD     11 / 4                flash/NVRAM
DEV_SRC      7 / 6                source/AV switch
DEV_PWM      6 / 2                backlight
ICE_*       2,7xx distinct         Novatek abstraction (CEC, audio, PQ, CEC SendData, ...)
```

So the hardware surface a "perfect replacement" must reproduce is roughly **12 device families**,
not one. Our current `ir` module touches exactly one of them, and it is the smallest.

## 3. Feature keyword volume (order of magnitude)

```
dlna 11557 · hdcp 5123 · tuner 4531 · teletext 4128 · pvr 3494 · epg 2298
picture 2259 · bluetooth 2176 · ci+ 1788 · volume 1574 · ota 1003 · netflix 916
alexa 455 · hotel 421
```

Read this as: the broadcast/DRM/interactive stack (tuner + EPG + PVR + teletext + CI+ + HDCP) is
the bulk of aurora, and none of it has anything to do with the home screen.

## 4. What this changes

My first module table was right about the *order* but too optimistic about the *size*. Three
corrections:

1. **Teletext / MHEG / Freeview Play (4,267 fns of strings and code) were entirely missing** from
   the plan. They are not "TV features" bolted on; `MHEGClient` is a first-class citizen.
2. **CI+/CA is a whole stack** (`CHIL`, `INCA`, `CAStackServer`, `CAStackController`, `CIStackProxy`,
   `ConditionalAccessIF`) — about 850 functions.
3. **Satellite/DVB-S exists** (`SatelliteFastScanServiceFinder`, `TunerManager`, `DEV_NIM`), i.e.
   this is not just a DVB-T set.

## 5. Redefining "perfect aurora replacement"

Strict parity means re-implementing a broadcast television. That is the wrong first target. The
useful definition is **tiered**, and the tiers should be decided by what the set is used for:

| tier | contents | when |
|---|---|---|
| **T1 — a working display** | `display`, `ir`, `power`, `panel`, `audio`, `source`, `settings`, `services` | required, always |
| **T2 — the TV features actually used** | `cec`, `apps`/browser, `pq`, `network`, `bt`, `teletext`/`mheg`, `tuner`+`epg`, `pvr`, `ci` | pick per household |
| **T3 — cloud/optional** | `portal`/NEBULA, `voice`/AVS, `hotel`, `global search`, `ota`, `frc` | probably never |

Given this set is currently driven as an HDMI monitor (Steam Deck, games console), T2's tuner,
EPG, PVR, teletext and CI+ stack are dead weight: they should be **explicitly descoped** rather
than half-implemented. That makes "perfect replacement" mean *"T1 complete and nothing the owner
actually uses is missing"*, which is achievable.

## 6. Cross-check of what we already built

| our module | aurora counterpart | evidence it matches |
|---|---|---|
| `mod_ir` | `DEV_UART_IR_*`, `DEV_Uart_GetKey*`, `_micom_receive` | 12 + 11 boundary calls; protocol fully recovered and implemented |
| `core` (loop) | `EASIControlTask`/`FROST_*` message queues and threads | we replace the thread-per-subsystem model with one loop, deliberately |
| `mod_ticker` | `TimeKeeper`, `NvramStorage` time services | trivial |
| `mod_ui_console` | `UIFrameworkControlInterface` (530 fns) | placeholder; the real work is phase 2 |
| — planned `display` | `DEV_TCON`, `PLAT_PQ`, `OsdHWHandle`, OSD order/alpha | matches the `ntkndpdrm` findings |
| — planned `audio` | `DEV_AMP_*` (82 calls), `AudioIF`, `ICE_Audio*` | matches, and shows amp init is the delicate part |
| — planned `settings` | `UserPreferences`/`ProfileConfiguration`/`NvramStorage` (2,067 fns) | much larger than a settings DB reader |

Nothing in the census contradicts the existing reverse-engineering reports; it only widens the
scope, which is exactly what this check was for.

## 7. Follow-up

* Decide T2 per feature and write it into `TODO.md` P8 (descope tuner/EPG/PVR/teletext/CI+ if the
  set is a monitor).
* `display` and `audio` are the two T1 modules with the most unknown surface; both should start
  from a Ghidra read of `DEV_TCON`/`PLAT_PQ` and `DEV_AMP` respectively.
* The browser (`OperaEngine`/`OperaBrowserCore`, ~400 fns) is the last T1/T2 decision: either keep
  using the vendor OMI engine as an `apps` backend or replace it.
