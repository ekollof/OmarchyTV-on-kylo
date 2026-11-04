# Live LauncherItem cards in ui_vendor — 2026-11-04

`ui_vendor` now reads the real parent-0 `LauncherItem` rows from the reloadable
`launcher.db` service instead of always drawing `DEFAULT_CARDS`. The rows are
filtered to type 2 with a non-empty thumbnail name, ordered by `m_order_index`,
and each card is looked up by its `m_application_thumb_file_name`. The explicit
`ui.cards` override still wins and `ui.live_cards=0` restores the static list.
The portal was captured offline and on the TV with the nine cards in the stock
order; the live TV database was also read and rendered by the module on the TV.

## Implementation

- `launcher.db` service gained a borrowed `oa_launcher_row_view` and a `get`
  callback, so a skin can use the rows without the control plane linking sqlite
  or the row layout. The list remains opaque and module-owned.
- `ui_vendor` `load_live_cards`: `read_parent(0,0,0)`, keep
  `item_type == 2`, skip empty or slash-containing thumbnail names, sort by
  `order_index` then `id`, cap at `MAX_CARDS`. Falls back to `DEFAULT_CARDS`
  when the service is absent, the DB read fails, or no rows match. The read is
  at module start; reloading `ui_vendor` refreshes it.
- The nine visible cards match the stock portal order from the dump:
  Netflix, Prime Video, YouTube, tabii, Canal Digitaal, TV VLAANDEREN,
  TÉLÉSAT, Digiturk Play, GAIN. The dump has 20 type-2 parent-0 rows; the
  sorted first 12 are selected and the first 9 fit the 1920 px row.

## Offline verification

`tests/ui_live_cards.c` uses an independent SQL oracle (parent 0, not deleted,
not out of sync, type 2, non-empty name) and creates OAI1 blobs named by each
selected thumb. Synthetic sizes encode the sorted position; with a real-asset
directory the OAI1 header dimensions are read and verified instead. It loads
only `nav_display`, `settings`, `ui_engine` and `ui_vendor` (no `plat`, so no
DRM/I2C side effects), paints through the recorder display, and asserts each
card blit at `x=209+i*190, y=478, 171x95` with the expected source dimensions.
Excluded rows are given a distinctive 200x200 blob and must not be painted.
The test removes its temporary directory with `nftw` (the first TV runs left
`/tmp/oa-live-cards.*` dirs; that leak is fixed and re-tested).

| case | rows selected | visible cards |
|---|---:|---:|
| synthetic (3 ordered rows, deleted/parent1/type1 excluded) | 3 | 3 |
| dumped `settingsDB.db` | 12 | 9 |
| live TV `/conf/sqlite_db/settingsDB.db` | 12 | 9 |

`offline-portal.png` is the 1920x1080 recorder capture for the dumped database
with the real card OAI1 blobs renamed to the live thumbnail names. It shows the
rail plus the nine cards in the order above.

## TV verification

Read-only recorder runs on the TV (ARM, `LD_LIBRARY_PATH=/vendor/lib/libs`):
synthetic 3/3, dumped 12/9, live 12/9, all exit 0, with no temp dirs left.

Visible display run: the `scene-module-probe` portal-hold mode loads `settings`
(with `settings.readonly=1`), `ui_engine`, `ui_vendor` and the other UI modules,
paints at 10 fps for 6 s and injects no keys. The first attempt used relative
broker/renderer paths and failed before EGL (`oa_scene_client_spawn` requires
absolute paths), retaining a framebuffer backup; the stock screen was captured
and confirmed intact. With absolute paths the portal was captured on layer 1:
`tv-portal-live_1.png` shows the nine cards in the same order. The probe then
reported `restored bytes=16588800 yoffset=1080 MAU=0 region_match=1
snapshot_match=1`.

A second visible pass using the live database reached UI readiness and the
6 s hold, but the capture showed the vendor's stock frame and the probe's
restore refused after the vendor's framebuffer region changed. The stock
no-signal screen was captured again and is intact; the retained backup was
removed. Do not claim a visible live-database capture from this run: the live
rows were rendered in the recorder test and read live, while the successful
visible capture used the dumped profile database.

Staging followed the owner's suggestion: `/buffer/oa_portal_20261104` and
`/buffer/oa_recorder` held the bundle and were removed; the helper backup in
`/tmp` was removed; no helper process or `/proc/*/exe` target remains. The TV
was asleep and `tvssh.sh` sent its usual WoL before the read-only runs. No
settings, network, boot or persistent deployment change.

## Limits

- The rail remains the static five tiles; only the card row is live.
- Assets are still OAI1 blobs named by thumbnail file name. Cards whose image
  is absent paint the existing gray fill; real downloaded-PNG decoding is not
  implemented.
- The read happens at `ui_vendor` start. There is no live-refresh event when
  `launcher.db` rows change; reload the module to refresh.
- The shipped `SQLiteDB::SequentialQuery`/`MemberAccess` engine is still not
  ported; the adapter is the recovered mapping over sqlite3.
- Scrollbar/skin, display/scanout binding, independent startup and full visible
  parity remain open. Nothing yet runs independently of vendor Aurora.
