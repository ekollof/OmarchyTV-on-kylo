# TODO accuracy audit — 2026-11-04

Subsequent state: [TODO-REAUDIT](TODO-REAUDIT-2026-11-04.md) accounts for the
later recorder fix and added PNG entry. Recorder alpha composition is closed
at its narrow boundary; PNG acceptance is reopened. The counts and decisions
below remain the historical review of432c71f, not the current checklist tally.

Audited the182 checked entries at432c71f against the current source,
maintained tests, firmware-reference results and saved topic evidence.
**One entry is reopened;181 remain checked as bounded milestones.66 retained
entries have corrected/narrowed wording.** The remaining unchecked acceptance
gates are still work, even when a related helper is implemented.

This is a source/test/evidence audit. Historical TV observations were checked
as dated evidence, not rerun or promoted into current independent hardware
acceptance. No TV access or hardware mutation was needed.

## Reopened: faithful offline layout capture

Original C002 described an RGBA layout recorder without its important alpha
limitation. tests/ui_nav_display.c's cap_blend forces the destination alpha
to255, including when blending onto an initially transparent pixel. The test
display does not publish display.alpha, so ui.engine uses the opaque fallback
for transparent clears. It emits useful opaque layout images; it cannot yet
capture the production canvas's transparent source-over behavior faithfully.

TODO now leaves that fidelity milestone unchecked. UI-FB-SIM is corrected at
the source. Its fbdev/Mesa feasibility and existing opaque screenshots remain
evidence; fixing the recorder is a separate implementation task.

## Corrections to retained checks

- The current row/list/image helpers exist and their native comparisons pass.
  Removed33 obsolete continuation tails claiming sibling switch cases,
  refresh/getter bodies, constructors or image-ID helpers were still absent.
  The common helpers are not all wired into live native widgets; those shared
  integration gates remain explicit above the list.
- Updated scrollbar coverage to14,576 geometry/state,32 skin and36 render
  cases. +6c is the four widget states, not orientation. The real divider,
  first-query calculation and second-query placement are present in source.
- Launcher field comparisons explicitly exclude the three advertising/tracking
  fields. Both SQL and object conversions enforce that exclusion; the checked
  helper is ordinary app/TV metadata conversion, not full vendor ad-field parity.
- Replaced the stale no-visible-apps claim with the actual sequence: the first
  GPU attempt failed, then OA-CANDIDATE captured a live-DB grid under stock
  ownership. The revised hardened display_fb has only loader/offline evidence,
  not a new draw test or full display restoration/startup acceptance.
- Corrected current private snapshots and service versions: ui_vendor writes
  five bytes and accepts the older four; ui.engine publishesv2 and rejects
  unsafe v1 registration; ui.navigation usesv2/OAN2 with OAN1 migration.
- Portal tests prove injected-event navigation, local screen changes and
  action emission. They do not prove the UART byte mapping, real app launch
  or source switching. Captured screens remain prototypes.
- Kept animation math/controller/highlight work checked only as the bounded
  implementation actually present. Starts while the timer runs retain phase;
  vendor restart-on-every-start and dynamic float-duration controller parity
  remain open. Rail playback uses a10ms poll,30ms target and receipt
  backpressure; a dated recorder mean is not physical scanout cadence or full
  higher-level image/widget lifetime proof. Removed the stale loop comment.
- The former “only six uncreated candidates” scan is historical. A later
  created title/render graph exists; the scans remain non-atomic observations.
  Stored localization, including dut slot10, is recovered; runtime language
  selection/fallback/shaping remains pending.
- --check's real16-module TV closure was a historical preserved production
  stage. It is not preflight of all modules now in source. artifacts/README
  now distinguishes that root stage from newer ABI3 development subdirectories.
- The workbench migration covers required durable inputs. Old screenshot/
  simulator defaults and payload scratch output still use /tmp/opencode;
  that is not a claim that every legacy helper is tmp-free. Recovery's exact
  script/image identities still match their local recorded hashes.

## Coverage of all original checked entries

Every original check has a stable C001..C182 identifier in the
[per-item inventory](../analysis/todo-audit-2026-11-04/checked-claims.md).
It records the original claim/line, current disposition and source/test/report
locators. before.md preserves the complete original TODO. inventory.cjs checks
that no original checkbox was silently dropped or reordered. Its locator
discovery does not automatically certify behavior; the reviewed family scopes
and execution evidence are recorded here.

| Original IDs | Reviewed boundary / anchors | Outcome |
|---|---|---|
| C001–002 | display_present, scene_gles, ui_nav_display | feasibility retained; transparent recorder fidelity reopened |
| C003–024 | core host/module/control/check, lifecycle/transport/quiescence/power fixtures, broker/mixer clients | bounded runtime/lifecycle checks retained; hardware/resource/standby rollback limits remain open |
| C025–034 | OSD/DRM/kernel reports, display_present, fbregion, vendor audio ABI | recovered contracts and dated reads retained; cold ownership/resume not claimed |
| C035–044 | power/panel headers, audio_curve/client/owner, AQ/profile tests and dated board reads | research/helper milestones retained; no native audible/amp ownership acceptance |
| C045–052 | CEC policy/packet/device/owner and route fixtures, transport/authority reports | spec/injected-I/O/query evidence retained; no independent bus ownership or ARC electrical claim |
| C053–066 | row-render/setter/image/constructor/widget-state helpers and shipped ARM fixtures | implementations retained; current aggregate counts supersede old milestones; native callees still bounded/controlled |
| C067–074 | launcher DB/conversions, apps tests, display_fb and OA-CANDIDATE | retained with privacy differences, current cache/state coverage and dated shared-scanout limits |
| C075–086 | renderItems cases and setType controller | all switch cases/helpers exist; removed obsolete missing-case lists |
| C087–090 | owned settings model, draw/refresh and animation-harness review | retained as owned-data/dispatcher/test-harness work, not native widget/rendering integration |
| C091–109 | common SVG list/tab/settings routers, bodies, state and animation helpers, ui.list | retained; native comparisons pass; no complete remote/UI consumer claim |
| C110–117 | settings.hotel, ui.profile, ui.navigation and launcher route plans | retained; currentv2/OAN2 migration distinguished from the originalv1 milestone |
| C118–125 | scanner, glyph routing/GPU primitive, title graph/origin/box and UI review | research/lower primitives retained; recorded raster discrepancies remain unresolved |
| C126–136 | extracted assets, pixel formats, ui_vendor prototypes and saved display evidence | retained; injected events/captured panels clearly distinguished from real TV feature parity |
| C137–157 | shadow edits, geometry, BinSVG/theme, font decode/layout/box and label research | retained at explicit input/style/ratio boundaries; fitted/live widget integration stays open |
| C158–161 | engine/skin selection, subscriptions and failure/fallback fixtures | retained; currentv2 owner-safe dispatch recorded |
| C162–169 | animation resources/math, ui.clock/ui.anim, slide/highlight prototype and scheduling records | retained as bounded/research milestones; controller/lifetime/frame/cadence parity stays open |
| C170–179 | canvas/scene IPC/worker/modules/receipts, transparency and saved GPU diagnostic | retained with initialized-layer and dated restoration/owner-observation scope |
| C180–182 | recovery hashes, documentation index and workbench migration | historical/provenance milestones retained; no current reboot/recovery experiment inferred |

## Fresh verification

Evidence is in ../analysis/todo-audit-2026-11-04/:

- host.txt: full strict make check passes, including actual Unix IPC,
  lifecycle/failure fixtures and current apps/renderer/loader tests.
- native-reference.txt: all42 firmware-reference groups pass through qemu on
  the identified work-review ARM executable. Source ELF/profile identities are
  checked by the maintained verifier before execution; the executable SHA256
  is printed. Controlled callees and explicit error/privacy policies remain
  distinct from native full-system behavior.
- vendor-corpora.txt: local shipped BinSVG/layout/font data, including all702
  records, all38 encoded fonts/52,022 glyphs, row/textbox composition and
  actual font7/16 line/text cases pass on the host.
- arm-build.txt and arm-corpora.txt: a fresh isolated build-tv-v3/todo-audit
  runs eight geometry/font/layout units under ARM qemu, including49,728 ARM
  VFP height comparisons, all52,022 glyphs,54 boxed lines and30 aligned lines.
- mesa.txt: the maintained surfaceless host-Mesa adapter passes all nine
  transform/orientation test types plus reopen/cleanup. This is an independent
  backend test, not stock/Mali pixel or visible cadence acceptance.
- recovery-identities.txt: the584-byte recovery script, tested current image
  and pristine /conf revert match the artifact index. Nothing was flashed.

No new acceptance is asserted for cold startup, device ownership, full
rendering/localization, real remote/app/source behavior, exact animation,
standby/wake, telemetry-free feature completeness or system replacement.
Those unchecked gates remain the route to the overall objective.
