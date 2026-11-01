# What aurora initialises (and in what order) — 2026-10-30

> **Status: Static initialization call inventory. Function counts depend on the analysis; call presence is not successful hardware bring-up.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Follow-up to `OSD-INIT-RE-2026-10-30.md` (display path). This is the rest of the boot: the app's own
initialisation. Source: `aurora.elf` symbols + Ghidra headless (project `/tmp/opencode/ghidra_proj`,
script `AuroraSeq.java`); dumps in `/tmp/opencode/ghidra_aurora_seq.log` and `init_order.txt`.

The binary keeps full symbols (90,822 functions), so this is readable rather than guesswork.

## Entry points

| function | calls | role |
|---|---|---|
| `NovoSystemInitialiser::initialise()` @ `0xf27a6c` | **388** named calls | builds every manager/service (the list below) |
| `NovoSystemInitialiser::initialiseNovoSystem(bool,bool,bool)` @ `0xf29e70` | **99** | system-level bring-up: portal, launcher, market, settings, tuner, DVR, **browser** |
| `initialiseBrowser()` @ `0xf235b8` | — | starts the OMI web engine — this is why `browserseturl` only works once boot settles |
| `processStartSystem()` @ `0xf2044c` | — | post-init: checks, first tune |

`main()` is only a thin CLI wrapper (argv parsing) — the real work is in those.

## Phase 1 — `initialise()`: services and managers (388 calls)

In construction order, grouped:

* **core plumbing**: `NovoCommandProcessor`, `NovoService`, `ApplicationServiceList`, `NovoDictionary`,
  `NovoTargetRegionController`, `NovoSourceStates`
* **config / storage**: `ProfileConfiguration`, `NvramStorage`, `UserPreferences`,
  `NovoStringLookUpTable`, `MediaBrowserConfiguration`, `NovoDateTimeConfiguration`,
  `NovoSystemTimeProvider`, `SqliteXMLChannelsDataAdapter`, `NovoNetworkSettings`
* **remote & input**: `NovoRemoteControl`, `InputManager`, `NovoExternalAVSourceNavigator`
  (this is what our `KeyAux`/Bronlijst path ends up in), `NovoPlatformSpecificSettings`
* **tuner / scan / DVB**: `TransponderConfiguration`, `NovoSatelliteHelper`, `TunerManager`,
  `NovoChannelScanController`, and one controller per operator profile (`FastScan`, `Polsat`, `Fransat`,
  `Tivibu`, `Tivusat`, `SimpliTv`, `Dsmart`, `Digiturk`, `CanalDigital`, `AstraHD`),
  `NovoTunerConfiguration`, `NovoDvbComponentSelector`, `NovoServiceList`, `NovoFilterView`,
  `NovoEITProvider`, `NovoServiceNavigator`
* **picture / sound / video**: `NovoPictureSettings`, `NovoSoundSettings`, `NovoVideoController`,
  `NovoSignalController`, `NovoAVComponentSelector`, `NovoThreeDStatusChecker`, `NovoLanguageSetting`
* **DVR / PVR**: `NovoDvrPresentationHelper`, `DvrLastPlayedStorage`, `AppRecordings`,
  `RetentionLimitChecker`, `EASIRecManagementAPIWrapper`, `DvrCAHelper`, `DvrHelper`
* **UI / OSD**: `NovoScreenViewController` (`registerScreenViewController`), `NovoHybridUIController`,
  `NovoControlledPopupManager`, `NovoFeatureOSDStatusChecker`, `NovoFrontPanelController`,
  `NovoScreenshot`
* **portal / apps**: `NovoPortalInformationProvider`, `ApplicationManager`, `ApplicationServiceList`,
  `registerNovoApplicationCapabilityProvider`, `NovoPortalAPIConnector`, `NovoDynamicOptionManager`
* **network / IoT**: `WiFiHelper`, `IoTManager`, `NovoAVSClient` (Alexa), `TVModelInfo`,
  `NovoBluetoothSettings`, `PubSubTVAdapter`, `NovoDeviceIdManager`
* **diagnostics / logging**: `NovoLoggingManager`, `NovoLoggingCommandProcessor`, `NovoDiagnostics`,
  `NovoTesttool` (+ `registerAppTestToolCommands`), `FROST_Time`, `TechSupportPreference`,
  `NovoTVLifeTimeChecker`, `BurnInMode`, `HotelModeConfiguration`, `NovoHotelModeSettings`
* **privacy/ads crew**: `NovoSambaACRController` (the ACR we block), `NovoKochavaClient`
  (attribution), `NovoRecommendationEngineController`, `NovoEnhancedMetadataProvider`

## Phase 2 — `initialiseNovoSystem()`: system bring-up (99 calls)

1. **portal & launcher**: `NovoPortalUrlManager::initialise`, **`NovoLauncherManager::initialise`** +
   `registerLauncherManager` — *this is the tile system whose DB rows we edited to add the OmarchyTV tile*;
   `VestelMarketClient::initialise` + `setNovoMarketClient`; `NovoKochavaClient::initialise`;
   GraceNote manager; `NovoGlobalSearchEngine` + `GlobalSearcherFactory`
2. **first-run settings**: `doInitialMediaBrowserFileSettings`, `doInitialNVRAMOperations`,
   `doCablePrefferedNetworkSettings`, `doSatelliteInitialSettings`, `doVideoInitialSettings`,
   `initialiseDualTunerConfiguration`, `applySoundSettings`, `setFrontPanelDisplayString`,
   `configureRepeatKeys`
3. **media stacks**: `initialiseDvr`, `setupSatelliteTables`
4. **browser**: `isBrowserEnabled` → **`initialiseBrowser`** → `notifyWebBrowserInitialisationComplete`,
   then the `NovoWebBrowserListener`s register (`registerWebBrowser` ×5, incl. YouTube data URL)
5. **misc**: `isVideoWallEnabled` / `readVideoWall` / `setVideoWall`,
   `isVirtualRemoteSupportEnabled`, `setRemoteControlSystemCode`, `initialiseInternetCheck`

## Where DIAL comes from

`DialController` is constructed from `initialise()` (near the end, alongside `IoTManager`,
`getWebsocketServerInterface` — that's the state WS on 7681). So our `browserseturl`/keys go:
dialserver → appmgr → `DialController` → `NovoLauncherManager` / `NovoScreenViewController` /
`InputManager`. That is why DIAL works before the portal paints (our opener uses it as a readiness probe
for exactly this reason).

## Display, in context

The OSD/plane setup is not in these lists: it happens inside DirectFB when the GUI initialises
(`DirectFBInit` → fbdev system → `libdirectfb_tdga.so`). Details in `OSD-INIT-RE-2026-10-30.md`.

## What this means for "replace aurora"

The ordered list is the answer to that question too: a replacement would have to supply tuner/DVB,
scan profiles, DVR, EIT/EPG, service lists, picture/sound/video control, HDMI/AV source handling,
Bluetooth, IoT, DIAL, the launcher manager, the browser, logging, hotel/burn-in modes… — i.e. the
whole TV. Replacing the *home screen* is cheap (we did it: a portal tile + a web app); replacing
`aurora` is re-implementing a television.

Practical takeaways:

* keep `aurora` as the engine and treat our launcher as a client of it (current design);
* the pieces we can safely hook without touching the engine: `NovoLauncherManager` (tiles, done),
  `DialController` (keys/URL, done), `NovoWebBrowserListener` (what the browser may load),
  `NovoScreenshot` (there *is* a screenshot path in the app — worth a look for remote captures);
* the pieces to leave alone: tuner/DVR/CI/CEC/AVS — they own hardware we cannot emulate.
