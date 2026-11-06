# Module inventory and consolidation — 2026-11-06

Scope: the 36 installed modules (`MODULES` in `openaurora/Makefile`), categorised
as hardware, middleware, utilities and UI/UX. Sizes are the ARM
`build-tv-v3/modules` `.so` files; the cold set is exactly
`tools/boot/openaurora-cold.conf`.

## Inventory

### Hardware — 12 modules, 873 KB

| module | KB | role | cold |
|---|---:|---|---|
| plat | 28.8 | HAL prelude, driver marker, platform services | yes |
| panel | 30.9 | panel curve/backlight | yes |
| display | 118.4 | fb/display ownership, alpha/patch | yes |
| graphics | 71.4 | GPU `display.gpu` backend |  |
| scene_display | 74.3 | GPU scene display backend |  |
| audio | 75.6 | audio HAL, speaker, CEC audio |  |
| cec | 94.6 | CEC ownership/arbitration |  |
| ir | 39.2 | input/IR (`input`) | yes |
| source | 23.1 | VIN/source switching |  |
| tuner | 278.8 | NIM/tuner/EPG ingest |  |
| spi | 20.7 | SPI transport, opt-in, not autoloaded |  |
| power | 17.3 | standby/resume |  |

These own devices or the HAL and have distinct claim/rollback/quiesce paths.
Merging them would couple hardware ownership and make a CEC or tuner reload
restart display/audio/panel. Not recommended.

### Middleware — 11 modules, 1840 KB

| module | KB | services | cold |
|---|---:|---|---|
| settings | 67.8 | settings, launcher.db, picture, hotel | yes |
| service_db | 40.0 | service.list (SQLite) |  |
| channels | 34.4 | channels |  |
| epg | 78.9 | epg, epg.ingest |  |
| pvr | 43.4 | pvr |  |
| image_decode | 43.7 | image.decode v2 | yes |
| i18n | 40.2 | i18n |  |
| audio_curve | 29.1 | audio.curve |  |
| audio_mixer | 45.8 | audio.mixer broker |  |
| audio_amp_profile | 42.5 | audio.amp_profile |  |
| lua | 1373.8 | `ui.lua` script state |  |

Most are independent data planes with their own reload/quiesce tests. `lua`
dominates the size; it is the moddability engine and stays.

### Utilities — 2 modules, 48 KB

| module | KB | services |
|---|---:|---|
| ticker | 25.1 | clock, tick topic |
| ui_clock | 23.1 | ui.clock (requires clock) |

`ui_clock` is the vendor UI logical clock over the raw ticker. They are always
started together, but both have independent reload/quiesce tests.

### UI/UX — 11 modules, 919 KB

| module | KB | services | cold |
|---|---:|---|---|
| ui_engine | 62.8 | ui.engine v2, ui.skin, display.alpha | yes |
| ui_vendor | 380.3 | vendor skin, portal | yes |
| ui_omarchy | 33.9 | native OmarchyTV skeleton |  |
| ui_theme | 49.6 | ui.theme |  |
| ui_profile | 42.3 | ui.profile |  |
| ui_menu | 75.2 | ui.menu |  |
| ui_list | 84.6 | ui.list |  |
| ui_navigation | 37.0 | ui.navigation |  |
| ui_anim | 46.2 | ui.anim |  |
| ui_console | 22.2 | test UI harness (stdout) |  |
| ui_diag | 84.3 | diagnostic status skin |  |

`ui_console` and `ui_diag` are explicitly test/diagnostic harnesses, not
product screens. The engine and skins must stay separate for runtime skin
selection; the model/widget providers are independently reloadable.

## Findings

- **36 modules / 3.68 MB installed.** Core is the hardware/platform base
  (10 modules): hardware access + settings/image_decode, no TV or UI layer.
  It is the SSH/build/control-plane base, not a functional TV.
- Distribution profiles add layers on top: `tv` (+5 TV services), `av` (+4
  audio/CEC), `ui` (+12 UI/UX/skin), `dev` (+3 diagnostics/SPI) and `full`
  (+2 alternative GPU backend). A functional TV is `tv`+`av`+`ui`.
- Every candidate leaf merge (`ticker`+`ui_clock`, `ui_theme`+`ui_profile`,
  `audio_curve`+`audio_amp_profile`) has an independent reload/quiesce test
  today. Merging deletes a tested reload boundary; it is a product decision,
  not a free build cleanup.
- A four-bundle layout (one `.so` per category) would give 4 files but needs
  either a multi-entry bundle ABI or a lifecycle rewrite, and reloading one
  feature would restart its whole category (CEC reload restarts tuner/audio/
  display). That contradicts the reloadable-module objective.

## Implemented: base platform + distribution stages (no reload change)

`openaurora/Makefile` now exposes the taxonomy and additive distribution
stages. Each stage is cumulative and its `requires` are satisfied inside the
stage:

| stage | adds | modules | bundle |
|---|---|---:|---:|
| core | plat, display, panel, ir, audio, source, power, tuner, settings, image_decode | 10 | 1.4 MB |
| tv | epg, pvr, channels, service_db, i18n | 15 | 1.7 MB |
| av | audio_curve, audio_mixer, audio_amp_profile, cec | 19 | 1.9 MB |
| ui | ticker, ui_clock, ui_anim, ui_engine, ui_vendor, ui_theme, ui_profile, ui_menu, ui_list, ui_navigation, ui_omarchy, lua | 31 | 4.2 MB |
| dev | ui_console, ui_diag, spi | 34 | 4.4 MB |
| full | graphics, scene_display (alternative display/GPU backend) | 36 | 4.5 MB |

`stage-bundle STAGE=...` writes the stage's `autoload=` list into the bundle
config, so each profile boots exactly its layer. `full` is a build/dev bundle,
not a boot profile: `graphics`+`scene_display` is an alternative to the core
`display` backend, not an additive layer. `cold-bundle-core` is the base
platform; the legacy `tools/boot/openaurora-cold.conf` 8-module set remains
the minimal UI subset used by the full bundle.

The startup autoload parser now accepts 64 names (was 16) and config values up
to 511 bytes (was 191), so the 31-module UI stage fits. `tests/autoload.cjs`
proves a 40-name list is not truncated.

Measured:

```text
hardware (12): plat panel display graphics scene_display audio cec ir source tuner spi power
middleware (11): settings service_db channels epg pvr image_decode i18n audio_curve audio_mixer audio_amp_profile lua
utilities (2): ticker ui_clock
ui/ux (11): ui_engine ui_vendor ui_omarchy ui_theme ui_profile ui_menu ui_list ui_navigation ui_anim ui_console ui_diag
core (10): plat display panel ir audio source power tuner settings image_decode
stage tv (15): core + epg pvr channels service_db i18n
stage av (19): tv + audio_curve audio_mixer audio_amp_profile cec
stage ui (31): av + ticker ui_clock ui_anim ui_engine ui_vendor ui_theme ui_profile ui_menu ui_list ui_navigation ui_omarchy lua
stage dev (34): ui + ui_console ui_diag spi
stage full (36): dev + graphics scene_display
core 10/1.4M; tv 15/1.7M; av 19/1.9M; ui 31/4.2M; dev 34/4.4M; full 36/4.5M
```

The full targets are unchanged, so the takeover queue keeps every feature
module available. The stage bundles are the distribution sets; a functional
TV is `STAGE=ui` (31 modules).

## Recommendation

1. Treat `core` as the base platform (hardware access, settings/image decode,
   control plane), then add `tv`, `av` and `ui` for a functional TV. Use
   `dev` for diagnostics/SPI and `full` for development/takeover staging.
2. Ship `cold-bundle-core`/`install-core` as the base image; distribution
   profiles are `STAGE=tv|av|ui|dev` (cumulative), with `ui` (31 modules) being
   the functional TV. Keep `install`/`cold-bundle` for the full set.
3. Do not merge hardware modules. Keep tuner, display, audio, CEC, panel, IR,
   source and power independently reloadable.
4. If fewer source modules are still wanted, do the leaf merges one at a time
   with explicit test updates:
   - `ticker` + `ui_clock` → `ui_clock` (utilities 2→1),
   - `ui_theme` + `ui_profile` → `ui_appearance` (11→10 installed),
   - `audio_curve` + `audio_amp_profile` → `audio_data` (11→10).
   Target 36→32 source, 33→31 installed. Each removes one independent reload
   boundary; only do it if that reload is no longer a product requirement.
5. Move `ui_console` and `ui_diag` (and `spi` when no NIM/CI consumer needs it)
   to dev-only installs; keep them buildable and test-covered.
6. Reject the four-category-bundle option unless coarse, category-wide reload
   is explicitly accepted; it is not a drop-in change.
7. Skins hot-switch through `api->control("skin <name>")`; the host defers the
   request until the calling skin callback has unwound (SKIN-SWITCH).

## Limits

- This is a build/packaging and inventory change only; no module lifecycle or
  ABI was modified.
- No module was merged or deleted. The 36 source modules still build and the
  full suite still covers them.
- Hardware acceptance is unaffected and still pending.
