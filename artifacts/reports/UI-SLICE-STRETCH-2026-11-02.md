# Horizontal stretch and composed list-row geometry — 2026-11-02

Firmware Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh ARM/decompile
and runtime evidence: artifacts/analysis/ui-slice-stretch-2026-11-02/.
Continues UI-LIST-METRICS. No new UI drawing or fitted-coordinate replacement.

## Ratio and transform contract

HorizontalMaskScaleTransform constructor eecb0c stores ratio1.0 as a double
at+28 (literal eecb80 is high word3ff00000), initializes empty surface/mask and
clears found+3c. getMask eecc24 lazily copies a resolved mask. When its width
is positive, nonpositive destination width sets ratio0; otherwise ratio is
double(destinationWidth)/double(maskWidth). A nonpositive mask width leaves
the previous ratio intact. resetMask eece68 clears mask/found but retains
ratio. setSurfaceUIRect eecbec copies corners without invalidating found.
Do not silently remeasure a cached mask merely because its surface changed.

transformPointRelativeToMask eecd0c subtracts mask origin. transformRect eecda8
and transformWidth eeccb4 use double conversions/multiplication and signed
conversion toward0. For a resolved mask:

    relativeX = input.left-mask.left
    scaledX = trunc(double(relativeX)*ratio)
    scaledWidth = trunc(double(input.right-input.left)*ratio)
    output.left = surface.left+scaledX
    output.right = output.left+scaledWidth
    output.top = input.top-mask.top+surface.top
    output.bottom = input.bottom-mask.top+surface.top

The relative X and width are truncated independently. Scaling both absolute
corners can differ by a pixel. Height is preserved; missing mask leaves the
input unchanged. This double width operation is separate from the manual
float32 text-height correction recovered in UI-TEXT-GEOMETRY.

common/text_layout.c now exposes explicit retained-ratio measurement and
rectangle stretching. OA_HORIZONTAL_SCALE_INITIALIZER supplies1.0.
Container lookup/found validity stays external; the API comment specifies when
measurement should run. Integer/float range and invalid-state failures are
bounded/atomic rather than firmware wrap/saturation. Default FP environment
is required. These APIs are not a complete resource-cache implementation.

## Composition from the actual477 resource

List/row renderBackground builds a horizontal middle destination from outer
corners by adding left-mask width to left and subtracting right-mask width
from right, keeping top/bottom. oa_horizontal_middle_surface implements that
construction (raw row/list extracts in UI-MASK-LAYOUT/UI-LIST-METRICS).

The test composes the shipped SettingListBoxUFA477 raw slices through the
maintained measurement/stretch/painted-metric/row-placement helpers. For an
explicit750x826 window, raw top16/bottom10/step70 and side widths0/1:

- First row surface is0,16..750,86.
- Middle destination is0,16..749,86.
- Raw rowPos equals raw mRowM (244,178..319,248). Its double stretch maps to
  the middle destination.
- Painted metrics become rowHeight70,itemWidth749,left0,right1.
- getRowPosition arithmetic reproduces0,16..749,86.

This is a composed resource-derived contract test, not a measurement of a live
widget instance. No screenshot-fitted origin feeds it. Resource parsing/group
selection, actual window inputs, row background graphics and full text layout
still need integration.

## Parent/client coordinates

SVGListBoxRow::create10bf35c copies the supplied row rectangle, then adds the
parent client left/top to both corners when a parent exists. UIWindow's
setWindowRect1152478 stores the final corners at object+28..34.
getClientRect1152974 copies that window, calls vtable+38 for a margin tuple,
adds margin left/top and subtracts margin right/bottom. The base
getClientMarginRect1152578 returns four zeroes. Confirm the relevant subclass
vtable binding before assuming every widget uses that base implementation.
These are static findings; no parent/window API was invoked on the set.

## Verification and continuation

tests/slice_stretch.c checks independent X/width truncation, preserved Y/height,
retained ratio, zero-width surface, missing mask, invalid/range failures and
the double-precision low bit beyond float32's exact-integer range. It includes
the477 composed path above. On ARM an independent inline VFP sequence performs
f64 int conversion/division/multiplication/truncation and compares49,538
rectangle cases (offset and width each).

Full strict host suite passes; focused ASan/UBSan for slice/list/text-layout
passes. ARM/qemu and native TV slice_stretch pass all49,538 VFP cases and
composition assertions. This proves arithmetic, not visible rendering/parity.

Native build-tv-v3/slice-stretch/tests/slice_stretch:46836 bytes,
MD5 d38cfe511c7b0b6f3aaa8ba31c57b92d, maximum GLIBC2.4. Separate successful
size/hash verification preceded execution. Test uses CPU/process memory only;
no devices or framebuffer. Staging removed and absence independently checked.
Same stock boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967,
hook/hosts, SSH22/telnet2323, Samba REJECT and safety markers. No persistent
deployment; no helper/build/Ghidra job pending.

Next confirm concrete client-margin binding, derive actual row/child windows,
then TextFactory/LineObj text origin/baseline/alignment. Connect a runtime theme
provider and complete layout to the UI seam before replacing fitted origins.
Do not repeat ratio/row arithmetic research or claim full widgets from these
helper tests. Title/tabs/focus/localization/value changes remain open.
