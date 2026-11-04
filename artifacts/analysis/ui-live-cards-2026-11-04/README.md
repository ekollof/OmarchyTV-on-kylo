# Live LauncherItem cards evidence

Interpretation: [UI-LIVE-CARDS](../../reports/UI-LIVE-CARDS-2026-11-04.md).

The settingsDB rows are read through mod_settings' launcher.db service; ui_vendor
keeps parent-0 type-2 rows with a non-empty thumbnail name, sorts by
m_order_index and loads the named OAI1 asset. No plat/hardware module is loaded
by the test.

```sh
make -C openaurora check-ui-live-cards check-ui-live-dump
make -C openaurora TARGET=tv BUILD=build-tv-v3/ui-live \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  build-tv-v3/ui-live/tests/ui_live_cards build-tv-v3/ui-live/tests/ui_nav_display.so \
  build-tv-v3/ui-live/modules/mod_ui_engine.so build-tv-v3/ui-live/modules/mod_ui_vendor.so \
  build-tv-v3/ui-live/modules/mod_settings.so
```

host-live-synth.txt / host-live-dump.txt: 3 rows/3 visible and 12/9.
offline-portal.png: recorder capture with the real card OAI1 blobs renamed to
the live thumbnail names.
tv-recorder-results.txt: the same synthetic/dump/live cases on the TV, all exit0.
tv-probe-dump-log.txt: visible probe portal hold on the dumped DB; restored
bytes=16588800 yoffset=1080 MAU=0 region_match=1 snapshot_match=1.
tv-portal-live_1.png: layer-1 TV capture of the nine live cards.
tv-probe-live-log.txt / tv-portal-live-db_1.png: live-DB visible attempt reached
the hold but captured the stock frame and the restore refused after the vendor's
region changed; the stock no-signal screen was verified and the stale backup
removed. tv-final-stock_0.png is the post-cleanup stock screen.

Staging was /buffer/oa_portal_20261104 and /buffer/oa_recorder, removed; no
persistent change. The TV was asleep and tvssh.sh woke it for the read-only runs.
