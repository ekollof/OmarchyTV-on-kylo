# Runtime BinSVG record view — 2026-11-02

common/binsvg.c is a maintained, borrowed immutable reader for one index-bounded
BinSVG record. It supplies validated element iteration and named rectangle
lookup, replacing the need to copy coordinates out of the offline TSV into
callers. It is not a pack loader, rendering engine or completed UI integration.

## Firmware contract

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh read-only/noanalysis
Ghidra output with ARM instructions is in
artifacts/analysis/ui-binsvg-view-2026-11-02/firmware.txt and lookup.txt.
Existing complete tag layouts are documented in UI-ANIMATIONS; rectangle
packing/read instructions are also in UI-TRANSPARENCY's theme-alpha.txt.

- BinSVGElement::read ef3aa0: byte tag then NUL-terminated ID, except group
  end9, which is one byte. Tags0..11 have the established primitive/group/
  animation layouts. Animation strings follow read, not the incompatible
  fixed-tail arithmetic in SVGAnimation::seek. The reader bounds every string,
  fixed field and trailing word; group nesting is also validated at init.
- SVGRect::read ef3b34 stores x/y/width/height/stroke width as packed16-bit
  fields, raw opacity byte, four strings and trailing begin word. Signed
  getters (e.g. ldrsh at ef2ac4 and ef2b2c) establish signed coordinates and
  extents. RectHandler ef6128 adds width/height to x/y to construct corners.
  Metadata remains raw: no color, opacity conversion or transform is guessed.
- RectHandler registers rectangles with nonempty IDs as descriptions rather
  than drawing them. RectangleContainer::addRectangle eed518 appends; its
  getRectangle eed2b8 scans from the start and returns the first match.
- getNamedRectangles ef2610 gathers named rectangles across the record.
  getNamedRectanglesOfGroup ef24a4 selects the first exact group name, includes
  descendants, tracks nested starts/ends, and stops at the selected closing
  tag. A later group with the same name is not searched for a missing rectangle.
  The new lookup preserves this distinction; NULL group means the whole record.

Init validates the entire record before publishing its view. Malformed input
leaves the previous view unchanged. Iteration and lookup return errno values
and preserve their outputs on failure. Views and returned strings borrow the
caller's immutable bytes. Empty IDs cannot be queried as named rectangles;
empty metadata strings remain empty strings. No recursion, device access,
file I/O, allocation, resource-name guessing or rendering occurs in this reader.
Mirroring and the existing mask/stretch transforms are separate caller steps.

## Verification and scope

tests/binsvg.c covers all12 tags and every byte truncation of constructed
elements, unknown tags, unbalanced groups, signed fields/negative extents,
duplicate names, nested groups, first-group-only behavior and unchanged error
outputs. check-binsvg is part of the full strict host suite.

Optional corpus arguments read the actual index/pack and validate all702
records, 11888 elements, 6733 rectangles, 39 animations and289764 bytes. Every
named rectangle is lookup-tested. The totals agree with the earlier offline
reader; this is structural/data validation, not vendor differential raster
acceptance. All shipped groups are balanced. A real477 resource lookup feeds
mTopL/mRowL/mRowM/mRowR/mBotL and rowPos into the maintained list/transform
helpers, reproducing step70/top16/bottom10 and painted row width749/insets0/1
for an explicit750x826 window. Slice coordinates come from the pack. That
window is still a test input, not a measured live widget instance.

Strict full host suite, focused ASan/UBSan synthetic/corpus, ARM/qemu and native
TV synthetic/corpus all pass. Isolated build-tv-v3/binsvg/tests/binsvg:
75356 bytes, MD5 4ecee7b4a67391ff35dcc22ac47ed2cd, maximum GLIBC2.7.
The live index/pack match the offline copies:

| file | bytes | MD5 |
|---|---:|---|
| telefunken18_svg.txt |10496|2ae154a9022eff003d70aeae2628e56d|
| telefunken18_svg.bin |289764|4d6f0bcee206bc1aefff5f548c764c9f|

A separate successful connection verified staged binary size/hash before
execution. Native test read only theme files and used CPU/process memory.
/tmp/oa-binsvg-20261102 was removed and absence independently checked. Same
stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967;
hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers unchanged.
No display/device/persistent operation; no helper or Ghidra/build job remains.

Next add indexed resource loading/ownership and a reloadable immutable theme
provider, then compose actual row/textbox state, transforms, viewport/clip and
font origins in the UI consumer. Existing captured panels and fitted label
origins remain unchanged. Whole-pack validation does not authorize or implement
advertising assets/functions; those remain excluded from the replacement.
