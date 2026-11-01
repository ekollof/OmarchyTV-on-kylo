# Rail/portal transparency mismatch — 2026-11-01

Status: owner-observed mismatch; initial firmware/source investigation only.
The background is not fixed yet. Existing screenshots/static RGB comparison
cannot establish transparency or composition over video.

The owner reports our rail UI has a dark background where the vendor UI is
transparent. Source confirms `ui_fb::paint` unconditionally clears the entire
display to RGB(14,14,13); display v2 clear has no alpha operand. scene_display
delegates to oa_canvas_fill, which writes alpha255. oa_canvas_blit likewise
flattens source pixels onto an opaque destination and writes alpha255. Merely
changing the clear color cannot fix this contract. The GLES owner already
clears its render target to transparent black and samples texture alpha; input
canvas opacity is a separate issue. CPU display must retain correct alpha too.

Fresh read-only Ghidra decompile plus ARM instructions from the shipped Aurora
MD5 64c252340d3af6c5d66072611b7bad7a establish:

- MainLauncherMenuDialog::showSubLauncherBackground e47f20 only forwards
  visibility to its child at +37f0; it is not a background-color/alpha setter.
- MainLauncherMenuDialog::init e4965c creates that child with image id0x2ce
  (e49dde/e49de4), initially hides it, and separately creates a child at +370c
  using id0x9c (e49e2e/e49e32). Trace image-resource lookup before equating integer
  ids to names solely from index order.
- SVGDialog::renderSVGData10c5104 delegates to the SVG renderer using the
  description at +110; it does not directly impose a full opaque color.
- RectHandler::handleSVGElement ef6128 extracts both fill and stroke properties
  and passes them to DrawingFacade. SVGRect::read ef3b34 reads the rectangle's
  opacity byte at fixed-data+10 into object+16 (ef3b70/72).
  SVGRect::getOpacity ef2bc8 returns it with **ldrsb** at ef2bdc, despite Ghidra's
  erroneous void prototype. Recover inheritance/range conversion in
  ShapeHandler::extractFillProperties ef6540 before interpreting that signed
  byte as an ordinary0..255 alpha.

Three background-named PNGs were carved offline using cumulative shipped-index
sizes, into ignored artifacts/workbench/ui-transparency-20261101. The two
11x11 mainmenu_background_tall/thin files have both transparent and nontransparent
pixels; tall includes alpha0,15,63,255. The100x100 sublauncher_background image
has alpha0 throughout. These are evidence that resource alpha matters, not proof
which image/layer supplies the owner's observed backdrop or its final opacity.
Recover the specific DialogMainMenu theme records and image lookup, then preserve
straight RGBA canvas and premultiplied BGRA scanout semantics end to end.

Pending: actual theme background/fill/inherited opacity, display/canvas ABI,
source-over composition on transparent destinations, CPU/GPU equivalence and
real panel/video acceptance. Do not adjust global OSD alpha by guess or apply
one opacity to the colored category tiles.

No additional TV changes or hardware probes were performed for this follow-up;
the preceding RAIL-ASSETS run restored stock framebuffer/access and removed all
staging. Raw evidence is in analysis/ui-transparency-2026-11-01.
