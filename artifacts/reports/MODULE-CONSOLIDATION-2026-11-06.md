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

- **36 modules / 3.68 MB installed; the cold config autoloads 8 / 772 KB.**
- 27 modules are not in the cold boot set; 4 of those (`ui_console`, `ui_diag`,
  `spi`, and the `ticker` example) are test/optional in nature.
- Every candidate leaf merge (`ticker`+`ui_clock`, `ui_theme`+`ui_profile`,
  `audio_curve`+`audio_amp_profile`) has an independent reload/quiesce test
  today. Merging deletes a tested reload boundary; it is a product decision,
  not a free build cleanup.
- A four-bundle layout (one `.so` per category) would give 4 files but needs
  either a multi-entry bundle ABI or a lifecycle rewrite, and reloading one
  feature would restart its whole category (CEC reload restarts tuner/audio/
  display). That contradicts the reloadable-module objective.

## Implemented: packaging tiers (no reload change)

`openaurora/Makefile` now exposes the taxonomy and additive deployment stages:

```text
module-categories   hardware 12 / middleware 11 / utilities 2 / ui-ux 11
all-core + install-core      core 8 modules
cold-bundle-core             cold config + 8 modules (1.5 MB bundle)
all-stage + install-stage + stage-bundle STAGE=core|tv|av|ui|script|diag|full
cold-bundle / install        full 36 modules (4.5 MB bundle)
```

Stages are cumulative. Each stage's `requires` are satisfied inside that
stage, and every stage starts from the same 8-module cold core:

| stage | adds | modules | bundle |
|---|---|---:|---:|
| core | plat, settings, image_decode, display, panel, ir, ui_engine, ui_vendor | 8 | 1.5 MB |
| tv | tuner, epg, pvr, channels, service_db | 13 | 2.0 MB |
| av | audio, audio_curve, audio_mixer, audio_amp_profile, cec, source, power | 20 | 2.4 MB |
| ui | ticker, ui_clock, ui_anim, i18n, ui_theme, ui_profile, ui_menu, ui_list, ui_navigation, ui_omarchy | 30 | 2.9 MB |
| script | lua (moddability layer) | 31 | 4.2 MB |
| diag | ui_console, ui_diag, spi | 34 | 4.4 MB |
| full | graphics, scene_display (alternative display/GPU backend) | 36 | 4.5 MB |

`full` is a build/dev bundle, not a boot profile: `graphics`+`scene_display`
is an alternative to the core `display` backend, not an additive layer. The
staged bundle still uses `tools/boot/openaurora-cold.conf`, which autoloads
only the core 8; extra modules are present for `load`/`load-batch` so a
core boot never depends on a feature layer being configured.

Measured:

```text
hardware (12): plat panel display graphics scene_display audio cec ir source tuner spi power
middleware (11): settings service_db channels epg pvr image_decode i18n audio_curve audio_mixer audio_amp_profile lua
utilities (2): ticker ui_clock
ui/ux (11): ui_engine ui_vendor ui_omarchy ui_theme ui_profile ui_menu ui_list ui_navigation ui_anim ui_console ui_diag
core (8): plat settings image_decode display panel ir ui_engine ui_vendor
stage tv (13): core + tuner epg pvr channels service_db
stage av (20): tv + audio audio_curve audio_mixer audio_amp_profile cec source power
stage ui (30): av + ticker ui_clock ui_anim i18n ui_theme ui_profile ui_menu ui_list ui_navigation ui_omarchy
stage script (31): ui + lua
stage diag (34): script + ui_console ui_diag spi
core bundle: 8 modules / 1.5M; tv 2.0M; av 2.4M; ui 2.9M; script 4.2M; diag 4.4M; full 36/4.5M
```

The full targets are unchanged, so the takeover queue keeps every feature
module available. The core/stage bundles are the recommended cold-start
staging sets.

## Recommendation

1. Start every deployment from the 8-module core, then add layers with
   `STAGE=tv`, `av`, `ui`, `script` or `diag` as the feature set grows. Keep
   `full` for development/takeover staging; it is not a boot profile.
2. Ship `cold-bundle-core`/`install-core` for the default cold deployment;
   keep `cold-bundle`/`install` for takeover/full-feature staging.
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

## Limits

- This is a build/packaging and inventory change only; no module lifecycle or
  ABI was modified.
- No module was merged or deleted. The 36 source modules still build and the
  full suite still covers them.
- Hardware acceptance is unaffected and still pending.
