# Evidence map

- render-items.txt: fresh SettingItemsListBoxRow::renderItems d54400, ARM plus
  decompile. Replaces the missing old /tmp extract; do not search for it.
- base-row.txt: SVGListBoxRow::renderItems10bf06c, setWindowRect10bf46c,
  drawRow10bf328. IDs18 and48 are explicit in renderItems instructions.
- render-group.txt: renderGroup ef2330 clears the output container and walks
  in stream order. The second query @eecc10 lands INSIDE setSurfaceUIRect,
  not getMask; do not label it a getMask extract. The actual getMask eecc24
  ARM/decompile is already durable in ../ui-text-geometry-2026-11-02/
  rect-mask-draw.txt. RectHandler ef6128 is in that report's rect-handler.txt.
- row-items.tsv/textbox-slices.tsv: raw offline reader output, not transformed
  widget geometry or live instance readings.
- host/sanitizer/arm and tv-* files: test/cleanup evidence, scope in report.

Standard-name pointer verification from the immutable ELF PT_LOAD bytes:
ID18: eee9ec -> 178df70 -> 153ae6f = mRowM (earlier UI-LIST-METRICS).
ID48: eeea64 -> 178e040 -> 153ae33 = rowItems (checked this turn).
First LOAD maps VA-file offset=0x10000; second=0x20000. All three reads
were file-backed, not guessed table offsets. Existing getWidgetStandardName
switch is in ../ui-resource-bindings-2026-10-31/oa-svg-hash-and-widget.txt.
