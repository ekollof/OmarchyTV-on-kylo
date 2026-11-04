# SVG widget state qualification — 2026-11-04

`common/svg_widget_state.c` now implements the widget-state/qualification path
`renderItems` uses on every child: `SVGWindow::setFocus` (`0xeeebd4`),
`SVGWindow::setActive` (`0xeee4ec`), the `SVGTextBox::setFocus` override
(`0x10c8a28`) and `SVGTextBox::setElipses` (`0x10c6f48`). **36** native/C
cases pass for the new widget-state group, and the full `verify.cjs` now has
**37** ok lines. Host unit, focused ASan/UBSan, ARM/qemu, full `make check` and
a focused real-TV CPU run all pass.

## Recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. All four bodies are Thumb code,
read with `--disassembler-options=force-thumb`.

The widget state is the 32-bit field at `+0x6c`. Values `0/1` are the
unfocused/focused pair in the inactive group; `2/3` are the same pair when the
window is active. Both setters first forward to `UIMessageTarget::setFocus` when
applicable and then remap only those states — unknown values are left untouched.

`SVGWindow::setFocus(this, focus)` (92 bytes, CRC `0x085d6c76`):

1. `UIMessageTarget::setFocus(this, focus)`.
2. `focus != 0`: `0 → 1`, `2 → 3`.
3. `focus == 0`: `1 → 0`, `3 → 2`.

`SVGWindow::setActive(this, active)` (84 bytes, CRC `0x7b1ef395`) — the
**argument is inverted** relative to the state:

1. `active == 0`: `0 → 2`, `1 → 3` (activates the state pair).
2. `active != 0`: `2 → 0`, `3 → 1` (deactivates it).

`SVGTextBox::setFocus(this, focus)` (80 bytes, CRC `0x117744b9`):

1. Save the state at `+0x6c`.
2. `SVGWindow::setFocus(this, focus)`.
3. If the state **changed** and `UIWindow::isCreated(this)`, call
   `SVGTextBox::drawTextBox(this)` then
   `SVGTextBox::performTextSlideAnimation(this)`. A repeated call with the same
   state still forwards to the base setter but does not redraw; an uncreated
   text box never redraws.

`SVGTextBox::setElipses(this, on)` (60 bytes, CRC `0x1606bd56`):

1. If `on` is true and the mode word at `+0x220` is `5`, return without
   storing.
2. Otherwise store `on ? 1 : 0` as a byte at `+0x1b1`.

So switching ellipses **on** is ignored in mode 5, while switching it **off**
still writes the byte.

New API in `openaurora/svg_widget_state.h`:
`oa_svg_window_set_focus`, `oa_svg_window_set_active`,
`oa_svg_text_box_set_focus`, `oa_svg_text_box_set_elipses`, with
`struct oa_svg_widget_state_ops` covering the vendor calls
(`UIMessageTarget::setFocus`, `isCreated`, `drawTextBox`,
`performTextSlideAnimation`).

## Verification method

`tests/svg_widget_state_native.h` copies all four bodies at their original
addresses, redirects their stack-guard literals, and installs register-
preserving stubs for the vendor calls. The grid is:

- `SVGWindow::setFocus` and `setActive`: 2 setters × 4 initial states × 2
  arguments = 16 cases.
- `SVGTextBox::setFocus`: 4 initial states × 2 arguments × created {0,1} =
  16 cases.
- `SVGTextBox::setElipses`: mode {9,5} × argument {0,1} = 4 cases.

That is 36 cases. Each compares the ordered call trace (base setter, isCreated,
draw, slide) and the resulting state byte/word after filling both objects with
`0xAA`, so a missing store is visible. The host unit additionally asserts the
exact transition tables, the no-change/no-redraw gate, the uncreated gate and
both elipses branches.

## Verification and limits

Isolated `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/svg_widget_state | 23336 | 417eef21aa10f02356edc929cddb7210 |
| tests/tab_policy_firmware | 1300760 | 21eb58423f8efa60054322648091d4b3 |

Full `verify.cjs` reports 37 ok lines including
`svg widget state: 36 native/C cases`. The focused real-TV CPU test ran the
self-contained `svg_widget_state` ARM unit: exit 0, staging hash-verified and
removed, no helper left, no-reset flag retained. No
device/screen/input/settings/network/service/boot change.

**Boundary:** `drawTextBox`, `performTextSlideAnimation`, `isCreated` and
`UIMessageTarget::setFocus` remain vendor stubs. The differential proves the
state remapping, argument inversion and redraw gating; it does not prove the
vendor redraw/slide pixels. Still open: live settings resources, scrollbar/skin,
display/scanout binding, independent startup and visible parity.

## Callee audit note

Plain `objdump` produces garbage for all four bodies; the Thumb flag is set in
the symbol address and `force-thumb` is required. This is the same trap already
recorded for the row constructor.
