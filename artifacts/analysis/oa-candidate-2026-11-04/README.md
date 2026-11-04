# Supervised no-plat candidate evidence

Interpretation: [OA-CANDIDATE](../../reports/OA-CANDIDATE-2026-11-04.md).

`mod_display_fb.so` is `modules/display/display.c` built with
`-DOA_DISPLAY_NO_PLAT`; it opens the vendor-initialized `/dev/fb1` and does
not require `OA_SVC_PLAT`. The candidate host ran
`settings,display_fb,ui_engine,ui_vendor` with the live
`/conf/sqlite_db/settingsDB.db`, `ui.screen=apps`:

```sh
LD_LIBRARY_PATH=/vendor/lib/libs /buffer/oa_candidate/openaurora \
  --modules /buffer/oa_candidate/modules \
  --config /buffer/oa_candidate/openaurora.conf --sock "" --log 2
```

`candidate-run.log`: `painted apps grid: 18 items, first 0, scrollbar 0/600`
from the live database. `candidate-apps_1.png` is the fb1 capture of the
apps grid and scrollbar. Vendor Aurora remained running (PIDs 1967/1785);
the candidate was stopped and `/buffer/oa_candidate` + `/tmp/oa_candidate.*`
removed. `after-candidate-stock2_1.png` is the restored stock no-signal
screen.

This verifies the supervised display/scanout binding to an existing
vendor-initialized fb1, not a cold independent startup. The cold run needs
the vendor stopped and `/buffer/wdtctl` watchdog disable (or `oa-supervise`);
the standing constraints mark that takeover unauthorized and a failed run may
need physical/USB recovery, so it was not executed. Real downloaded-PNG
decoding and stock apps-grid pixel parity remain open.
