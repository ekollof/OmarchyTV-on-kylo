# Bitmap font selection — 2026-11-01

Continues UI-FONTS. Firmware Aurora MD5 64c252340d3af6c5d66072611b7bad7a.
Raw decompile/ARM and test evidence: artifacts/analysis/ui-font-selection-2026-11-01/.
Current milestone is pure selection/metric code; widgets still use captured text.

## Font IDs, heights and startup sets

UIFrameworkTask::getFont114698c forwards the ID to UIBitmapFont::getFont115e3e0,
which directly indexes the decoded resource array. The font IDs below are the
byte-pack record indices, not Unicode IDs or point sizes. BitmapFont::getExtremes
b42438 returns the metrics at object+4. The vtable symbol starts013f9338;
the object vptr starts013f9340, whose slot+8 resolves that getter.
UIWindowBuilder::getMaximumTextHeight1152f98
loads signed bottom/top bytes and subtracts: extremes.bottom - extremes.top.
common/font_bitmap.c now exposes that calculation as oa_bitmap_font_height.

DemoApp::addFonts cc52e0 first clears the finder and registers:

| type | registration IDs (in order) | corresponding maximum heights |
|---|---|---|
| 0 |18,17,16,7,6,5,4,3,2,1|80,75,68,61,56,50,43,35,30,24|
| 1 |22,21,20,14,13,12,11,10,9,8|81,74,67,63,56,50,42,34,24,19|

All20 heights are checked against the actual pack. The other18 EF41 records
are decoded data, but are not added by this startup registration. Do not invent
family names (regular/bold/etc.) for the numeric types without further evidence.
Theme font24's extreme height wraps to-10; it is not part of these startup sets.

## Selection rules and implementation

UIFrameworkFontFinder::addUIFrameworkFont eef0c0 obtains the above maximum height,
caps total registrations at42, appends to global/type arrays and stably sorts by
height (eeef74 / eeefe4). Equal heights retain registration order.
getFont eeedc0:

- Empty finder returns ID0, before division.
- FUN01285478 is signed integer division, not min/max. getFont supplies its
  height and line-count arguments; SVGTextBox::renderForeground10c7a00 passes
  its text rectangle height and field+1b4 (also used as the drawText line count).
  Result truncates toward zero. This is the available height per line.
- Types0..5 select their populated set. Empty or unknown types use type0.
- findMatchFont eeed60 initializes its result from the globally smallest ID.
  In the selected sorted set, it takes the last font whose positive height
  does not exceed available height. Nonpositive height or the first too-large
  height ends the scan. Below the selected set's minimum, the global result
  remains. Thus type0/23px returns ID8 (type1), rather than ID1 (type0/24px).

common/font_finder.c provides this policy separately from skins/rendering,
with stable ordering,42-entry capacity and output-preserving errors. It uses
explicit bounds instead of reproducing vendor findMatchFont's unused typed-count
argument/global-count/sentinel read. The shipped populated entries and zero
terminator semantics are preserved; uninitialized/out-of-bounds reads are not
a desired compatibility contract. The public API rejects division by zero and
INT_MIN/-1 with EDOM instead of executing the vendor's arithmetic fault path.

Examples:48px/one line chooses4(type0,43px) or11(type1,42px);100px/two lines
chooses5 or12 (both50px). These are metric-selection results, not panel evidence.

## Validation and limits

Strict C11/-Wall/-Wextra/-Werror/-pedantic host, focused ASan/UBSan and ARM/qemu
pass. Full make check exits0 (existing249 indented checks plus two font groups).
Synthetic tests cover type fallback, global fallback below minimum, stable ties,
line division, nonpositive metrics, unknown type registration, capacity and
invalid arithmetic. The optional font_bitmap corpus test reads all38 fonts,
verifies the20 startup heights and12 selected-font cases. It is data-derived
policy testing; the vendor finder was not called as a differential oracle.

Both native tests pass on the TV; the corpus uses live /vendor2 files. Separate
successful size/hash verification preceded execution. Native artifacts live in
build-tv-v3/font-selection; preserve the prior font-bitmap build separately:

| executable | bytes | MD5 |
|---|---|---|
|font_bitmap|37748|b4afdf02bc01759f17107257dd5c5204|
|font_finder|24240|c476f17d4f3d747e4ec4b429f55b16ff|

No vendor/device APIs were invoked; tests read fonts and run pure code. All
temporary staging removed and independently checked. Same boot71f7eb63-727f-
4b9a-a9c7-2b04dc3f9197 and stock liveAurora1785/zombie1967, hook/hosts hashes,
SSH22/telnet2323/Samba REJECT and safety markers. No hardware/display change.

Next recover the SVG text-style type/line-count/rectangle fields for each row,
baseline/alignment, text controls, shaping/substitution, clipping and color
blending. Then integrate font data and text drawing through the UI engine seam.
Do not replace captured labels with guessed type/size/family, or close focus,
localization or native rendering parity based on this selector. No helper or
Ghidra job remains pending.
