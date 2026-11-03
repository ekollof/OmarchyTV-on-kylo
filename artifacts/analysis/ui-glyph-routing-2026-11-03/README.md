# Glyph dispatch and parent alpha evidence

Interpretation: ../../reports/UI-GLYPH-ROUTING-2026-11-03.md.
glyph-route.txt and execute-route.txt are fresh read-only/noanalysis ARM and
decompile; callers.txt lists precise renderText/doExecute call sites.

```
node artifacts/analysis/ui-glyph-routing-2026-11-03/verify-route.cjs
```

Uses prior saved non-atomic context bytes and shipped ELF, not new process
reads or a target function invocation. Private raster inputs and output are
in ignored workbench/title-horizontal-20261103; no font or screenshot is added
to git. Native output gpu-title-opaque.rgba has8294400 bytes and MD5
be8e8e0ae0abe3e2aeb1ee9937fa4614. Core mask extraction uses the previous
UI-FONT-GPU README recipe with this output:1318 white pixels/1072 differences.

Explicit test invocation, after separate verified staging:

```
egl-glyph-probe FONT_INDEX FONT_BINARY OUTPUT_RGBA 178,29,43,255
```

RGB comes from saved title-background samples; alpha255 is an explicit test
condition, not independently proved for that title position. Full parent paint
stack and visible parity remain open. Host/sanitizer/native logs are here;
tv-cleanup.txt confirms staging/helper absence and unchanged display/access.
