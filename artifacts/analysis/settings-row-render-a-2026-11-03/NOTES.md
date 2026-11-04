# renderItems type 0xa — partial recovery (not implemented)

> Superseded: SETTINGS-ROW-A-2026-11-03.md in artifacts/reports records the
> completed created/uncreated type0xa implementation and native comparisons.
> This is an earlier failed fixture, not a pending task or a reason to retry
> that crash. TODO's stale orphaned partial-recovery paragraph was removed by
> WORK-REVIEW-2026-11-04.

Status: **not implemented**. The not-created paths below were read from the
ELF with objdump and are believed correct, but the created slider path
(`0xd559d0`) is only partly recovered and the differential still crashed, so
no code was committed. This note exists so the mapping is not lost.

## Jump table

Type 0xa (10) -> body `0xd5575c`.

## Recovered flow

1. `getRectangle("slidebar")` via literal `0xd55a0c` (slot `0x178d620`) -> r8.
2. `getRectangle("text2")` via literal `0xd55a10` (slot `0x178d5dc`) -> r9.
3. If r9 == 0 skip the text box (`0xd55822`).
4. Text box is `row+0x1ca8`; `isCreated`.
   - Uncreated (`0xd557ac..0xd557fa`): names **0x1e8/0x1e9/0x1ea/0x1eb**
     into slots **1/0/3/2**, `setFocus(sl)` and `setElipses(sl)` with the
     isCreated result (0), then
     `SVGTextBox::create(sl, "", rect+8, 1, row)` using the char* empty
     literal at `0xd55a14`. **No setFont** and no key1 branch (unlike type
     0x9, which uses 0x26d + font 0x1e when key1 is present).
   - Both paths then build `UString("")` and `operator<<` `row[0x2df8]`,
     `setText`, destructor (`0xd557fe..0xd5581e`; created path re-enters from
     `0xd55892` after copy/point/offset/`setWindowRect`).
5. If r8 == 0 return (`0xd55822`).
6. Slide bar is `row+0x1480+0x18 = row+0x1498`; `isCreated`.
   - Uncreated (`0xd55842..0xd55890`): names **0x214/0x216/0x215/0x217**
     into slots **1/0/3/2**, `SVGSlideBar::setFocus(sl)`, then
     `SVGSlideBar::create(sl, row[0x2df0], row[0x2df4], rect+8, row)` and
     `setSliderValue(row[0x2df8])` — this joins the type-0x9 create at
     `0xd556e4`.
   - Created jumps to `0xd55c38` -> `b.n 0xd559d0`, **not yet recovered**.
     A partial read of `0xd55c3c` shows a copy/point/offset +
     `setWindowRect` + `UString("") << row[0x2df8]` + `setText` sequence,
     which looks like a text-box refresh rather than a slider refresh, so the
     block boundaries there need re-reading before use.

## Difference from type 0x9

| aspect | 0x9 | 0xa |
|---|---|---|
| text-box names | 0x26d + font 0x1e when key1, else 0x1e8..0x1eb | always 0x1e8..0x1eb |
| text box child | row+0x1ca8 | row+0x1ca8 |
| slider child | row+0x13c0 | row+0x1498 |
| slider names | 0x26c (key1) | 0x214/0x216/0x215/0x217 |

## What blocked it

With the uncreated paths implemented the differential still crashed inside
the created slider branch, because that branch is un-stubbed/unmapped. The
sweep must either implement `0xd559d0` or exclude `created` for type 0xa
until it is recovered; skipping the case would have understated coverage, so
the work was reverted rather than committed half-verified.
