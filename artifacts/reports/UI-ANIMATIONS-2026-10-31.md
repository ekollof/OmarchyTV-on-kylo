# Vendor UI animation contracts — 2026-10-31

Status: **current offline firmware evidence**. Extractors, ui.clock and the
list/dialog controller are tested. Text slide, full-screen visual mapping,
rail timing and on-panel acceptance remain unfinished. No TV access is claimed.
Full UI parity includes these effects; static screenshots are insufficient.

## Sources and tooling

Aurora MD5 64c252340d3af6c5d66072611b7bad7a, from this TV's vendor volume.
Ghidra Aurora project, read-only/noanalysis, DumpFirmware.java with instructions.
telefunken18_svg.bin MD5 4d6f0bcee206bc1aefff5f548c764c9f;
its index MD5 2ae154a9022eff003d70aeae2628e56d. Local copies:
artifacts/ui-assets/_layout. Raw evidence and animation TSV:
../analysis/ui-animation-2026-10-31. Do not redistribute firmware-derived content.

`openaurora/tools/re/svg-animations.c` walks the indexed pack structurally with
bounds checks. Build/run from openaurora:

```sh
make BUILD=build-strict build-strict/svg-animations check-svg
build-strict/svg-animations ../artifacts/ui-assets/_layout/telefunken18_svg.txt \
    ../artifacts/ui-assets/_layout/telefunken18_svg.bin
```

It consumes all **702 records / 289,764 bytes**, walks **11,888 elements**, and
extracts **39 animation descriptors**. The numbers cover this pack, not every
vendor animation path. Index record lengths, typed elements and NUL-terminated
strings define boundaries; string-search hits alone are not descriptors.
Non-animation elements are skipped using recovered layouts, not fully decoded.

## Packed descriptors

BinSVGElement::read (0xef3aa0) reads a tag byte and NUL-terminated id. Type-name
table at 0x1501c0c maps tags 0–11 to line, rect, image, circle, ellipse, polyline,
polygon, path, g, /g, animate, animateTransform. Group-end is one byte, with no id.
The prior claim that these names are length-prefixed was incorrect.

SVGAnimation::read (0xef3ec4) reads after the common id:

1. Animation-type byte and signed 16-bit animation offset.
2. NUL strings: xlink, attribute name, transform type.
3. Signed 16-bit begin value.
4. NUL strings: duration, fill, from, to, calcMode, keySplines.

Getter bodies at 0xef340c–0xef3628 confirm field identities and signed short
returns. Empty strings become null pointers in vendor objects; TSV keeps them
empty. The subtype/offset/begin values are preserved, not assigned guessed policy.

Of the 39 entries, 38 specify duration `0.3` and calcMode `linear`; one specifies
duration `1` and an empty mode. There are 29 animate and 10 animateTransform
entries. Examples include scales `1 1` → `1.35 1.35` and `1.08 1.08`, glyph x/y
offsets, and selection-indicator y motion. SVGTextBox::getAnimationDuration
(0x10c7620) multiplies parsed duration by 1000 and takes the maximum across its
container. List-selection start (0x10bbedc) also converts a matched descriptor's
duration to milliseconds.

The resource-name follow-up below resolves all hashes to exact style names.
These examples still do **not** prove the portal category rail uses any particular
scale or duration. The pack is only one source; other effects are implemented in
widget/renderer code.

SVGAnimation::seek (0xef4214) uses a fixed 32-byte tail after its initial strings,
unlike read's variable-length fields. The extractor follows read for animations
and obtains exact pack boundaries. Do not substitute the seek arithmetic or
conclude it causes a visible vendor bug without tracing its actual callers.

## Resource-name and widget-state binding follow-up

DynamicSVGSupplier::createContainer (0xef58e4) hashes the requested name through
0xef57bc: `crc32(0, name, strlen(name))`. Case and the `.svg` suffix are included;
the terminating NUL is excluded. DynamicSVGSupplier::getSVG (0xef575c) takes an
integer resource ID and fetches that name from the exported `svg_name_list`.

That ELF object is at 0x157d364 and has size 2808: **702 ARM pointers**. The new
bounded `openaurora/tools/re/svg-names.c` locates it through dynamic symbols,
translates addresses through file-backed PT_LOAD segments, reads bounded strings
and checks each name's CRC32 against the corresponding pack-index entry. All
702 names match, in index order; all target hashes are unique. This supplies
authoritative resource IDs/names without geometry-based guessing.

```sh
make BUILD=build-strict build-strict/svg-names check-svg
build-strict/svg-names /tmp/opencode/rootfs/vendor/com.nvt.idtv/aurora.elf \
    ../artifacts/ui-assets/_layout/telefunken18_svg.txt
```

Result: ../analysis/ui-resource-bindings-2026-10-31/oa-svg-names.tsv. Joining it
to the previous animation TSV names all 39 descriptors; the named/annotated
result is `named-animations.tsv` in that directory.

| style family | extracted effect, all at 300 ms unless noted |
|---|---|
| launcheritemFA/FI | scale 1→1.32; glyph x 0→−34, y 0→−37 |
| launcheritem_service_button_FA/FI | scale 1→1.35; glyph x 0→−34, y 0→−37 |
| sublauncheritemFA / sublauncheritem_fvc_FA | scale 1→1.35; glyph x 0→−42, y 0→−24 |
| fosditemFA / fosdbgitemFA | scale 1→1.35; glyph x 0→−34, y 0→−37 |
| SettingHorizontalListBox FA/FI/UFA/UFI | selectionIndicator y 0→100 |
| SettingSubLauncherListBox / SettingRecommendationsListBox FA/UFA | selectionIndicator y 0→100 |
| TVBannerRecommendationsItemFA | scale 1→1.35; glyph x 0→−42, y 0→−24; content/backend policy still requires review |
| animatedImage | imagex x 0→40 over 1000 ms; caller still unverified |
| sublauncherAdBannerFA | **excluded advertising**, research only: scale 1→1.08, x 0→−32, y 0→−10 |

The advertising row accounts for three descriptors. Do not port its UI/backend,
even optionally. Other recommendation-related names alone do not establish that
a content source is permitted or forbidden; review actual behavior against the
standing telemetry/advertising exclusions before implementation. The extractor
is a read-only research inventory, not a runtime feature allowlist.

SVGWindow::getStateName (0xeee5e4), isActive (0xeee570), setActive (0xeee4ec),
and setFocus (0xeeebd4) establish independent active/focus axes:

| internal WIDGET_STATE | meaning | suffix |
|---|---|---|
| 0 | active, unfocused | UFA |
| 1 | active, focused | FA |
| 2 | inactive, unfocused | UFI |
| 3 | inactive, focused | FI |

Active is `state < 2`; toggling active preserves focus, and toggling focus
preserves active. Do not conflate inactive with unfocused or automatically with
disabled. setSVGName (0xeee5ac) stores a resource ID at `this+0x84+4*state`.

MainLauncherMenuDialog::getCategorySvgIndex (0xe47d68) uses state remap
**{1,0,3,2}** at 0x14efbb7 before indexing category arrays. The arrays themselves
hold FA/UFA/FI/UFI, a different order from internal WIDGET_STATE. Their values
resolve as follows (resource IDs are firmware IDs, not language or remote codes):

| category code | family | UFA / FA / UFI / FI resource IDs |
|---|---|---|
| 0 | myMenuLauncheritem | 581 / 579 / 582 / 580 |
| 1 | liveTvLauncheritem | 593 / 591 / 594 / 592 |
| 2 | settingsLauncheritem | 589 / 587 / 590 / 588 |
| 3 | appsLauncheritem | 585 / 583 / 586 / 584 |
| 4 | sourcesLauncheritem | 597 / 595 / 598 / 596 |

The exported m_*_svg_indexes arrays at 0x178d78c–0x178d7cc provide the values.
Category families are distinct from generic launcheritemFA/FI, so generic tile
scale parameters cannot simply be copied to the category rail. Geometry update
functions (0xe48924/0xe48aec/0xe47de4/0xe47e70) use named descriptions and
conditional category presence; their full behavior still needs integration.

The name tool's host, ASan/UBSan and ARM/qemu output is byte-identical. Synthetic
tests cover standard/firmware CRC vectors, dynamic-symbol string bounds, segment
translation, BSS/EOF rejection; the CLI rejects a modified index hash. The public
runtime core/modules were not rebuilt or changed by this tool work (one stale
palette comment was corrected). Raw source hashes/instructions/literal pools
and validation logs are in the resource-bindings directory.

## Controller, interruption and easing

The constructor (0x11396dc) initializes animation enablement to **false**.
setAnimationEnable (0x11397e8) only writes the flag; it does not itself cancel
entries or reschedule timers. Recover the bootstrap callers before choosing
native enablement policy.

UIAnimationController::startAnimation (0x1139898) checks enablement, initializes
its message target, and rejects durations below 1. An existing entry for the
same target gets a new start timestamp/duration rather than a second entry.
The target receives initial progress/elapsed zero. The old timer is killed and
an immediate timer is requested: delay **0**, message id **0x96**.

onAnimationTimer (0x1139aa8) computes elapsed time, divides by duration, clamps
completion to progress 1, calls the target with progress and elapsed, removes
completed entries, and requests the next timer after **10 ms** while entries
remain. Renderer callbacks bracket the update/present work. This is scheduled
cadence, not proof of 100 on-panel frames per second. **0x96 is not 150 ms.**

cancelAnimation (0x11399e0) erases an entry without forcing its final state.
finaliseAnimation (0x1139a30) first applies progress 1 and elapsed=duration if
needed, then erases it. Repeated keys, lost focus and interrupted animations must
preserve the distinction; a generic cancel-and-snap implementation would differ.

Target callbacks also transform progress:

- SVGListBoxBase::onAnimationProgress (0x10bb6c4) linearly interpolates rectangle
  coordinates with integer truncation.
- SVGDialog::onAnimationProgress (0x10c4dac) passes **1+(p−1)^3**, capped at 1,
  to full-screen animation progress. This is cubic ease-out.
- SVGDialog::startAnimation (0x10c5008) uses a **1000 ms** literal at 0x10c5094,
  conditioned on full-screen renderer support, modal depth and animation type.
  It maps selected dialog type codes to the renderer's full-screen type. Those
  codes still need semantic/caller mapping; it does not animate every dialog.

Thus linear pack metadata must not be generalized to all transitions, and the
full-screen easing must not be applied to list selection or text motion.

## UI clock prerequisite — implemented offline

The controller uses FROST_Time (0x744270) and FROST_TimeSubtract (0x7443b4),
not ordinary unbounded monotonic milliseconds. ARM instructions establish:

- CLOCK_MONOTONIC milliseconds are truncated to 32 bits before normalization.
  Values strictly greater than **90,000,000 ms** are reduced modulo that period;
  equality is retained. Elapsed subtraction adds that period on rollover.
- Elapsed raw time accumulates in a remainder. When it is strictly greater than
  **12,343**, its quotient by 12,343 increments a correction, and its remainder
  is retained. The correction itself is reduced only when greater than the period.
- The returned time subtracts the correction, using the same rollover rule.

Literal pools at 0x744358–0x744374 and unsigned division/modulo helpers at
0x1285200/0x128545c confirm the constants and arithmetic. Preserve the strict
comparisons; replacing them with >= changes boundary behavior.

`include/openaurora/ui_time.h` implements this arithmetic. New `mod_ui_clock`
publishes `ui.clock` v1 with now_ms/elapsed, holding ticker's existing `clock`
service. It samples on queries and an owned one-second timer, saves only versioned
numeric clock state for reload, removes the timer on suspend and restores it on
resume. Ticker now has paired suspend/resume hooks for its own sampling timer.
The raw 64-bit clock service and public module ABI remain unchanged.

The one-second idle sampling policy is our implementation choice. Vendor global
initialization/call history and absolute clock phase are not fully recovered;
the module starts with its own zero state. Arithmetic agrees for supplied sample
traces, not necessarily the live vendor process's absolute timestamps. Very long
gaps spanning complete periods are not reconstructed by this algorithm.

Seven actual-DSO regression groups cover golden correction/rollover boundaries,
ARM32 truncation, reload and snapshot rejection, failed-resume retry, missing
dependencies/timer exhaustion/publication collisions, real ticker dependency
ordering and idle sampling. Strict host, ASan/UBSan and ARM/qemu runs pass.
Raw instructions and validation logs: ../analysis/ui-clock-2026-10-31.
This supplies the clock. The controller itself is the later mod_ui_anim section.

## Text scrolling

AnimationHelper constructor (0xeefc54) initializes pause **800 ms** and divisor
**50**. calculateSlidingDuration (0xef0210) uses signed integer division before
converting to float: **1600 + trunc(distance/50) × 1000**. The division helper at
0x1285478 and the 800/1000 literal pools were checked in instructions. This is
quantized duration, not a guessed continuous pixels-per-second formula.

calculateSlidingMargin (0xef0264) holds zero through the initial pause. During
the middle interval it computes a clamped fraction over duration−2×pause and
truncates distance×fraction to an integer; at/after total duration it returns to
zero. A reverse flag returns distance−margin. Caller constraints on distance,
short movement intervals and directional behavior still need integration work.

> **Correction 2026-11-01 (re-read instructions at 0xef0264):** the last clause is wrong for the
> forward direction. `margin = (int)(distance × frac)` with `frac` clamped only above, so at/after
> total duration the margin is **distance**; only with the reverse flag (`distance − margin`) does it
> end at zero. The same re-read confirmed the helper members from the constructor at 0xeefc54 and the
> literals: pause `800.0f` at `0xeefc8c`, divisor `0x32` (50), multiplier `1000.0f` at `0xef0260`.
> Implemented in `common/ui_anim.c` (`oa_anim_slide_duration`/`oa_anim_slide_margin`), published as
> `ui.anim` v2, with host/ASan-UBSan/ARM-qemu assertions in `tests/ui_anim_review.c`.

SVGTextBox::performTextSlideAnimation (0x10c7768) checks animation enablement,
sliding state, focus, text mode and whether sliding is needed. It may finalise
instead of starting. Its progress callback (0x10c8e48) also arranges a **2000 ms**
repeat timer on a particular sliding-state/completion path. Keep the repeat
pause separate from the helper's 800 ms edge pause.

## Validation and remaining implementation

Host, ASan/UBSan and ARM/qemu readers produce byte-identical TSVs and consume the
entire actual pack. tests/svg_animations.c verifies signed fields and golden
output, group-end/rectangle boundaries, unknown tags, and every truncated prefix
of synthetic animation/rectangle fixtures. A truncated actual pack is rejected.
All builds retain C11 -Wall -Wextra -Werror -pedantic. The initial extractor work
did not change runtime behavior. The subsequent UI clock module/ticker hooks
are built and tested offline only; installed/recovery files remain unchanged.

## Offline list/dialog controller, 2026-11-01

`common/ui_anim.c` and `mod_ui_anim` publish `ui.anim` v1 and require `ui.clock`.
Enablement starts false. `set_enabled` accepts only 0 or 1 and does not cancel
entries. `start` rejects a duration below 1 and a list call without both
rectangles. The same id replaces its slot. There are four slots. `cancel` erases
without a callback. `finalise` applies progress 1, notifies done, then erases.
List coordinates use the truncating blend from 0x10bb6c4. Dialog progress is
`min(1, 1+(p-1)^3)` from 0x10c4dac. Select, when enablement is on, starts dialog
id 1 for 1000 ms. The module timer is delay 0 and repeat 10 ms; `sample(now)`
is that update and does not sleep. `mod_ui_fb` binds the callback: a finished
dialog opens the selected tile, and a list sample paints the current rectangle
in the accent color. That is not a recovered mapping of
`setFullScreenAnimationProgress`. The category rail is still instant. Text-slide
pause, duration and repeat are not called. The 0.3 s pack duration is not copied
onto the rail.

`tests/ui_nav.c` drives the shipped modules. It passed host `make check`, one
ASan/UBSan run (`build-owner-san`) and one ARM/qemu run (`build-owner-arm` with
the TV `/lib` namespace). No framebuffer, UART, CEC or amplifier was opened.
Installed and recovery binaries were not rebuilt.

## Text-slide caller wiring, 2026-11-01

`SVGTextBox::isTextSlidingNeeded` (0x10c76b0) resolves the overflow in pixels:
it measures the label through the context font finder / window builder and
stores `text_width − box_width` at `+0x2ac`, the value the duration and margin
helpers consume. The framework samples at 10 ms (`setTimer(...,10,10)`).

`SVGTextBox::onAnimationProgress` (0x10c8e48) tests the progress against `0.99`
(literal at 0x10c8ee8) and the sliding state against 3; when both hold it calls
`UIFrameworkTask::setTimer(target, 2000, 200)`, whose 2000 ms operand becomes
`CabotTimerStart(handle, 2000)`. That is **one 2000 ms shot**, not a repeating
timer: the pass ends held at the margin, the shot later restarts it at margin 0.
The "200" is the timer-call cadence operand and is not the restart delay.

`mod_ui_fb` now mirrors that: the title blit is drawn at `29 − margin`, the
slide is only started when the loaded title is wider than `ui.slide_box`
(default 0 = disabled), a 10 ms sampler repaints while running and is deleted at
completion, and a single `OA_ANIM_SLIDE_REPEAT_MS` timer re-arms the pass. The
math and the runner live in `common/ui_anim.c` (`oa_slide_start`/`_offset`/
`_running`) so the module carries no timing copy. `tests/ui_slide.c` drives the
shipped DSO with a settable `ui.clock` through the full sequence: pause, half
span, end at distance, sampler stop, 2000 ms re-arm, second pass. Host `make
check` (195 ok), ASan/UBSan and ARM/qemu all pass. This is offline caller
evidence; no live TV visual check has been made, and the repeat was not
observed on the panel.

Remaining here: the rail-specific duration (the 0.3 s pack value is still not
recovered as the rail's own timing), the `setFullScreenAnimationProgress`
mapping, and independent frame sequences.

## Full-screen animation mapping, 2026-11-01

`UIFramework::setFullScreenAnimationProgress` (0x113bd48) forwards through
`UIFrameworkTask` (0x1146b84) to `UIAppClient::setFullScreenAnimationProgress`
(0x95f0cc), which calls vtable slot 0x28 of the client's HAL object. The type
setter uses the same chain with slot 0x20 (0x113bc94 / 0x1146b14 / 0x95f05c);
`isFullScreenAnimationSupported` uses slot 0x24.

The only UI callers are in `SVGDialog` (Ghidra reference query):

- `SVGDialog::startAnimation` (0x10c5009) returns for mode 0x38, requires the
  HAL to report support, an existing app and `UIApp::modalTargetsDepth() == 0`,
  then maps modes 0x39..0x40 through the byte table at 0x153b80c
  (`01 02 03 04 05 06 07 08`) to HAL types 1..8. Outside that range it sets
  type 0. It then starts the dialog controller for 1000 ms (0x447a0000 at
  0x10c5094) on `this + 0x98`.
- `SVGDialog::onAnimationProgress` (0x10c4dac) computes
  `min(1, 1+(p-1)^3)` — the same cubic already used for dialog progress — and
  passes it to the framework setter.
- `SVGDialog::destroy` (0x10c4e10) forces progress 1.0 when the dialog target's
  `getAnimationProgress` is still below 1, then tears the dialog down.

The HAL consumer is `hal::OpenGLGraphicsContext` (type at `+0x78`, progress
clamped to 0..1 at `+0x74`, dirty flag at `+0xa4` while changed).
`iterateFullScreenAnimation` (0xb32910) rewrites vertex positions; the pool at
0xb32aa8 holds π, 2/3 (0x3f2aaaab), 1/3 (0x3eaaaaab) and 1/6 (0x3e2aaaab):

| type | vertices | transform |
|---|---|---|
| 0 | none | no geometry; no uniform update |
| 1 | 1, 2 | x × (0.5+0.5p), y × (2/3+p/3) |
| 2 / 4 | 0..3 / 4..7 | x − (1−p)/6 |
| 3 / 5 | 0..3 / 4..7 | x + (1−p)/6 |
| 6 | none | no geometry, uniform only |
| 7 | 0..3 | x,y × (2/3+p/3) |
| 8 | 0..3 | x,y × 1 + (1−p)·sin(π·6p)/3, with 6p widened to double for the π multiply |

Types 4/5 select the second vertex group (`param_1 + 4` entries of 0x18 bytes);
types 1, 7 and 8 touch only the entries listed. The GL uniform set at the end of
the function is not needed for geometry and its decompiled argument order is not
trusted, so it is not claimed here.

Encoded as `oa_fs_apply` (`common/ui_anim.c`, `openaurora/ui_anim.h`) with exact
unit vectors for every type at p = 0/0.25/0.5/1, both slide directions and both
vertex sets, progress clamping and short/NULL vertex arrays. Host `make check`
(now 196 ok), ASan/UBSan and ARM/qemu pass. Applying the transform to the
renderer's whole-scene composite is still open; nothing calls it yet.

Next: a rail-specific duration, applying the recovered full-screen transform in
the renderer composite, and independent frame sequences. Keep RGBA asset inputs
and BGRA framebuffer output distinct. On-panel timing remains unverified.
