# Single-line textbox composition — 2026-11-02

common/widget_layout.c now composes a fresh normal, non-mirrored, single-line
textbox's text surface, numeric alignment and font type from supported actual
resource layouts. It joins the row-window helpers to textPos/font selection.
It does not yet select the actual live parent instance, draw glyphs, replace
the fitted UI coordinates or establish focused-text visual parity.

## Source and branch contract

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Existing durable ARM/decompile:
analysis/ui-text-layout-2026-11-01/text-widget-surface.txt, renderBackground
10c5e8c, especially10c5fba..10c63c4; analysis/ui-text-geometry-2026-11-02/
rect-mask-draw.txt contains renderForeground10c7a00 and create10c8888.
UI-TEXT-GEOMETRY, UI-ROW-WINDOWS and UI-FONT-SELECTION own the lower contracts.

For lines==1, the background takes ONLY the horizontal row slices: gRowL with
mRowL/alignment1, gRowR with mRowR/alignment2, then gRowM with mRowM and a
horizontal mask stretch. The middle surface removes the side-mask widths but
keeps the FULL widget height. It does not remove top/bottom cap heights; those
belong to the other branch. TextPos is horizontally transformed, then its
height is independently corrected with float32 widgetHeight/rawMaskHeight.
Foreground offsets this description by the widget window to create its text
surface; the font finder receives that corrected text-surface height.

Alignment-marker priority is left group ->4, otherwise right ->6, otherwise
any full-record alignment marker ->5, default4. Font markers choose first of
regular/bold/light/italic/boldItalic/lightItalic, default regular0. The new
helper models fresh boxes with no explicit font/alignment override. Forced
font IDs and retained override/cached widget state remain caller work.

Supported textbox styles must put mRowM before textPos in gRowM, as488/489 do.
The ordered group helper supplies the transformed textPos and the raw first
mask. A pre-mask textPos explicitly returns ENOTSUP: depending on later
elements, renderGroup may or may not have cached its mask before the subsequent
full-record description pass clears the container. Applying one pass blindly
would invent behavior. Complete second-pass/cache handling for those layouts
is still open. Other missing structures fail rather than using guessed geometry.

Fresh constructor/caller extracts are in
analysis/ui-textbox-compose-2026-11-02/constructors.txt: SettingItemsListBox
defaults row-items460, PictureSettingsDialog chooses dialog179
DialogSettings_tab.svg. The latter's raw listBox is(20,170,750,826).
This identifies constructor resources, not every runtime override or a measured
live object. mb-row-items.tsv is only a comparison record667: its existence
does not show this screen uses it. No MB profile selection is inferred.

## Implemented result and tests

oa_widget_single_line_textbox returns absolute text-surface corners, borrowed
raw rectangle metadata, alignment and font type. It checks bounds/denominator
and preserves output on failure. It performs no font loading, drawing,
clipping, shaping, glyph-origin calculation or hardware operations.

tests/textbox_layout.c covers48 combinations of alignment markers/font types,
left/right/global priority, regular-over-bold priority, top/bottom caps that
must not affect a single line, raw metadata, missing structure, pre-mask
rejection, zero mask height and atomic coordinate/range errors.

The live-pack test composes resource460 text1 for the explicit conditional
parent(20,186,769,256), giving child(30,187,381,237), then resources489/488:

| state | absolute text surface | height | font type | selected font |
|---|---|---:|---:|---:|
| UFA489 |31,182,380,248|66|0 regular|7|
| FA488 |31,181,380,251|70|1 bold|20|

Font selection uses all20 startup registration IDs with heights decoded from
the actual live font pack. This is conditional data/geometry/selection evidence,
not proof those are the coordinates/font of a currently rendered live instance.
No fit-derived translation is added. Horizontal glyph origin, vertical widget
alignment mapping/ratio, actual parent placement and final draw clip still need
to be joined and independently checked before changing UI origins.

In these two records the white/#7E000A colors are the raw **stroke** strings;
fill is `none`. Do not accidentally read the fill attribute as the text color.
This helper preserves both raw strings; final drawText color conversion/use is
separate. Recoloring captured text still cannot prove focused-font parity.

Strict full host suite, focused ASan/UBSan, ARM/qemu and native TV synthetic plus
live-record/font-metric corpus pass. Native build-tv-v3/textbox/tests/textbox_layout:
124016 bytes, MD5 d1e11762fe72fc0d2314731bd953e2ad, maximum GLIBC2.7.
Separate successful size/hash check preceded execution. An initial passing
binary was replaced by the final build adding explicit pre-mask rejection;
tv-cleanup-initial.txt records its cleanup, final tv-* logs describe this hash.

Tests only read live SVG/font files and use CPU/process memory. All staging
removed and absence/processes independently checked. Same stock boot71f7eb63-
727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts, SSH22/telnet2323,
Samba REJECT and safety markers. No display/device/settings/persistent changes
and no pending helper/build/Ghidra job.

Next establish the actual row/window chain and drawText/TextRenderer glyph
origin/ratio/alignment/clip, then replace fits and validate FA/UFA text on the
display. Preserve the captured fallback until that chain is complete.
