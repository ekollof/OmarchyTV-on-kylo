# Bitmap glyph drawing and SVG text markers — 2026-11-01

Current milestone: one bitmap glyph into an offscreen RGBA canvas, plus recovered
font-marker semantics. No UI label replacement, baseline/layout or focused-row
pixel parity is claimed. Owner requested finishing this item, handoff and stop.

## Firmware evidence

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Raw decompile/ARM:
artifacts/analysis/ui-text-layout-2026-11-01/. Earlier UI-FONTS and
UI-FONT-SELECTION supply EF41 records and generic selection.

DrawContext::drawGreyScaleGlyph b43ea0 adds signed left/top bearings to the
supplied font origin, walks raw coverage with tight stride=max(right-left,0)
and signed height, skips zero coverage, and clips to the supplied rectangle.
The origin is not established as a UI baseline. The draw-context slot+0x18
used for each covered pixel is ARGB8888ColourAPI::compositePixel b370c0 for
that color API. Its prototype metadata is wrong: ARM r1=destination,
r2=source-color pointer, r3=coverage. Source alpha0 leaves the pixel unchanged.
Constant b37188=0x80808081 implements unsigned /255 via high multiply and >>7.

For nonzero source alpha/coverage:

    a = floor(source_alpha * coverage / 255)
    weight = a * 255 + destination_alpha * (255 - a)
    output_alpha = floor(weight / 255)
    output_channel = floor((source_channel * a * 255 +
                            destination_channel * destination_alpha * (255-a)) / weight)

Zero combined alpha clears the full pixel. This includes a nonzero source alpha
whose effective alpha truncates to0 over a transparent destination with stale
RGB. Existing oa_canvas_over already supplies the nonzero source-over arithmetic;
the glyph wrapper handles this zero-alpha corner explicitly. Canvas storage is
straight RGBA. The display backend still performs final premultiplied BGRA
conversion; no blanket framebuffer channel swap is added here.

## Settings text marker correction

SVGWindow::getWidgetStandardName eee798 resolves marker IDs41..46 through
pointer literals eeeaec..eeeb00 and the strings near153acff..153ad3c.
SVGTextBox::renderBackground10c5e8c tests these in priority order and stores
the type at object+1c4 unless an explicit override is present (+225):

| type | marker |
|---|---|
|0|fontRegular|
|1|fontBold|
|2|fontLight|
|3|fontItalic|
|4|fontBoldItalic|
|5|fontLightItalic|

The first present marker wins; none selects0. These are firmware type names,
not proof of an external font family or synthetic emboldening. Only0/1 are
populated by DemoApp::addFonts; empty type sets fall back as UI-FONT-SELECTION
describes. oa_font_type_from_markers implements the priority for consumers.

The verified BinSVG rectangles (subset preserved as settings-text-rectangles.tsv):

| resource | textPos | font marker |
|---|---|---|
|488 settingListTextboxFA|9,2,40,42; #7E000A|fontBold|
|489 settingListTextboxUFA|9,3,40,40; #FFFFFF|none|
|496 settingListButtonFA|279,102,87,68; #7E000A|fontBold|

These are serialized resource coordinates, before widget transforms. They are
not final screen positions or a measured baseline. At one line and unchanged
text height, label FA42/type1 chooses font11, while UFA40/type0 chooses font3.
Actual line-count/setup and transformed geometry still need tracing, so those
conditional calculations are not proof of live per-row font choice.

The earlier recolored focused overlays cannot establish vendor focused text:
the resource changes font type and text rectangle as well as ink. Keep the
reconstructed-overlay milestone and its pixel comparison separate from parity.
The historical script remains a reconstruction helper, with a correction note.

## Implementation and validation

common/font_draw.c / include/openaurora/font_draw.h provide one glyph from a
validated borrowed font view at an explicit origin, optional canvas-intersected
clip and RGBA color. Bounds/coordinates use int64_t; invalid font/glyph/canvas
returns an errno value before drawing. Nonpositive signed glyph extents are
blank. No allocation, I/O, shaping, cursor advance, text parser, line layout or
font storage ownership is hidden in this primitive. Font data must remain
immutable and must not overlap the writable canvas.

tests/font_draw.c covers signed bearings, transparent output, clipping, integer
extremes, invalid glyph/span/view and every65536 color-alpha/coverage pair over
a translucent background, plus the zero-alpha/stale-RGB corner. The corpus
test now renders every52022 glyph from all38 shipped fonts into bounded scratch
canvases and compares every pixel to the stored coverage/transparent border.
That proves our coverage mapping, not vendor-vs-ours text raster output.

Strict C11/-Wall/-Wextra/-Werror/-pedantic full host suite exits0 (existing249
indented checks plus three font groups). Focused ASan/UBSan synthetic and full
corpus pass; ARM/qemu corpus and glyph tests pass. All three native executables
pass on the TV, reading live font files and drawing only in process memory.
Source hashes and existing font records are those in UI-FONTS.

Native build-tv-v3/font-draw (separate from the two prior font builds):

| executable | bytes | MD5 |
|---|---|---|
|font_draw|51308|3613fb79434422436f8518c1a976dc88|
|font_bitmap|60924|9200d8ca98f437c51cae895913546b84|
|font_finder|25428|f53444b73e2bf02880bce15dd5e0bceb|

Separate successful size/hash verification preceded execution. Temporary
/tmp/oa-font-draw-20261101 removed; absence and stock state independently
checked. Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, liveAurora1785/zombie1967,
hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers. No device
or framebuffer opened, settings changed, service restarted or persistent deploy.
No TV helper, build or Ghidra job remains pending.

## Next agent

Continue actual row text setup/line count and mask-transformed textPos geometry;
then font origin/baseline/alignment, spacing, ellipses, substitution/shaping and
text clipping. Fresh extracts include SVGSurface::drawText eedfe4,
UISurfaceImpl::drawText115ffec, TextCommand::renderText1131f94,
drawCharacter1131a90 and LTR helper nextCharacter1131670. The LTR helper uses
64ths and getGlyphAdvanceWithKerning; b42394 returns signed advance<<6, not
pair kerning for these bitmap fonts. Do not infer full string shaping/layout
from that helper. No full text renderer/service or font-data provider exists
yet; wire them through the UI engine seam once the contracts are established.
Visible focused labels and complete vendor UI parity remain open.
