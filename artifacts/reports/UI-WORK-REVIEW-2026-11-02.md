# Review of subsequent settings UI work — 2026-11-02

Reviewed changes from5fbf9e4 through6822fce: focused text/slider diagnostics,
title rendering/geometry/localization, lifecycle subscription failure, scanner
extensions and documentation. Fixed two implementation defects and filled test
and handoff gaps. This review performed no TV operation or persistent deployment.

## Fixes and verification

- The scanner accepted a0x224-byte SVGTextBox while its new inventory read
  override+22c, slide margin+2a8 and slide byte+2b0. A standalone short-buffer
  regression reproduces an ASan heap-buffer-overflow. The inventory now requires
 0x2b1 readable bytes (the read span, not a claim about class allocation size).
  Exact-minimum/one-byte-short/old-minimum buffers and extended signed fields
  are covered, along with the existing chunk-boundary/embedded-row tests.
- ui.edit_init was applied with ui.edit disabled. Presets now require the same
  explicit ui.edit=1 gate as key-driven edits. Tests cover disabled presets/keys,
  enabled presets/keys, trailing junk, negative/over-range/long decimal values.
  The invented0..100 range remains a default-off diagnostic, not vendor parity.
- The title-disabled fallback test existed but was absent from the normal make
  target. It now runs alongside title-on, missing fonts and both preset cases.
- The subscription-refusal test covered a full table only. It now also frees
  exactly one slot so the first registration succeeds and the second fails;
  both failures release holds/subscriptions, and a later start/unload succeeds.
  The prior fail-start implementation passed that additional case unchanged.

Full strict host make check passes. Focused ASan/UBSan scanner/settings-text/
subscription groups pass after the reproduced scanner failure. Matching ARM
hard-float TV-target builds pass those focused groups under qemu-arm-static with
the durable glibc sysroot, including all five settings-text modes. Maximum GLIBC
requirement of the reviewed vendor module is2.17, below the TV's2.23; this and
qemu execution are not native hardware/visual acceptance.

ARM artifacts in openaurora/build-tv-v3/review-20261102:

| file | MD5 |
|---|---|
|modules/mod_ui_vendor.so|e407989e9a19f056cfc81021d85bd347|
|tests/ui_object_scan|822f8c3eb3f0ebcd75d6b91091893787|

Research scripts independently rerun successfully: MODE and all six tab-title
keys resolve in the hash-checked2435-record/41-locale profile. These are stored
translation checks, not proof of current instance text.

## Evidence corrections and continuation

The prior live-scan file contains a created font18 candidate with window
(57,5,691,95), local text=(51,0,634,90), lines1/type0/alignment4. Those fields
agree with the conditional composed box. It also records actual list metrics
and row/digit boxes. This advances the old heap scan that had no created title;
the top-level handoff had not incorporated it.

The scan is non-atomic and lacks a synchronized object/frame association and
the later draw-control fields (the second scan caught the screen closed).
It narrows the title investigation; the statement that it DEFINITIVELY isolates
the entire ink delta to drawText was too strong and is corrected at its source.
Do not add a fitted(+23,+5) offset. ui.title stays default-off; plain glyph
drawing is not full textbox layout. Focused text currently changes ink using
the same font, so it does not establish the vendor's FA font/style parity.
The live row's70px frame supports the186+70*N origin, but the complete focused
widget/slider composition remains open.

Preserved the current4.7MiB /tmp/opencode research tree into ignored durable
artifacts/workbench/agent-ui-review-20261102, including decompiles, screenshots,
helper source and binaries. Referenced extant decompiles now point there. The
older decomp-textfg.txt was already missing; its existing warning remains.
No missing file is claimed recovered. Review logs are under that snapshot's
review-logs; these are dated evidence, not the next experiment queue.

Next: trace the actual title draw-control/ratio/spacing path from the shipped
ARM instructions before changing placement; integrate recovered row/text styles
and interaction before claiming visual parity. Preserve optional lab flags and
existing module ownership. Cold startup, audio/CEC and other full parity gates
remain open; no vendor replacement was performed.
