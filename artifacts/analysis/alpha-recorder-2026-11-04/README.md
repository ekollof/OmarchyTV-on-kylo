# Alpha recorder fidelity evidence

The TODO-AUDIT-2026-11-04 reopened item was the offline recorder forcing
destination alpha to255 and not publishing `display.alpha`. `tests/ui_nav_display.c`
now:

- composes `cap_blend` with `oa_canvas_over` (straight RGBA source-over,
  destination alpha preserved);
- provides `clear_rgba` and publishes `OA_SVC_DISPLAY_ALPHA`, so `mod_ui_engine`
  no longer falls back to an opaque clear for transparent screens;
- keeps the in-memory raster current so `nav.trace.capture(path)` works even
  when `OA_NAV_CAPTURE` was not set before drawing.

`tests/ui_sources.c` checks the integration: the 780x1080 source panel is
opaque, the right side stays alpha0, invalid clear alpha is rejected, 50% white
over opaque blue gives `(128,128,255,255)`, 50% green over transparent gives
`(0,255,0,128)`, and an untouched pixel remains `(0,0,0,0)`.

## Files

- `host-check.txt`: full strict `make check`,273 ok.
- `sanitizer.txt`: focused ASan/UBSan `check-ui-sources`; LeakSanitizer is
  disabled because `mod_ticker`'s pre-existing `m_init` allocation is not freed
  by this test (unrelated to the recorder).
- `arm-qemu.txt`: the same test under ARM/qemu (`mod_plat` built with the
  host-port `-DOA_NO_VENDOR`; no TV or hardware access).
- `capture-run.txt`, `capture-alpha.txt`: six recorder frames from the test.
  Portal frames have alpha153 (0.6 at the corner); source-selection frames have
  mean alpha0.40625 (=780/1920) and corner alpha0.
- `source-selection-alpha.png` preserves the alpha channel;
  `source-selection-flat.png` is the same frame flattened on `#404040`.

This proves production-canvas layout fidelity only. It is not vendor-Mali pixel
parity, scanout, or a replacement for the remaining native integration gates.
