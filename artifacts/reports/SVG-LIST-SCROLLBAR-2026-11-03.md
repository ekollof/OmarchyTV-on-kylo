# SVG-list scrollbar construction — 2026-11-03

`common/svg_list_scrollbar.c` implements SVGScrollBar::create (thin forward
to the common construction step) and SVGScrollBar::setScrollPosition (store
gating plus conditional background/foreground render calls). 294 native/C
comparisons pass with exact call order/args and resulting bar state, plus
strict host, focused sanitizers and ARM/qemu. The owner resumed the goal
after the SVG-LIST-IMPL stop; no TV access, wake, staging, deployment or
pending job.

## Firmware contract

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Native reference fragments:

| address | bytes | CRC32 | body |
|---|---:|---|---|
| 10c401c | 52 | 52a07cab | SVGScrollBar::create |
| 10c4138 | 112 | 671cda0c | SVGScrollBar::setScrollPosition |

Only guard-pointer literals are redirected; isCreated, renderBackground,
renderBar and the common construction step are controlled stubs. No vendor
object initialization is involved.

create forwards (created,rect,parent) unchanged. setScrollPosition stores
vis/size/first at +0xbc/+0xc0/+0xc4 iff the bar is created and signed first
does not exceed signed vis; it renders the background only when the +0xc8
byte is clear, then always renders the bar; otherwise it stores and renders
nothing. The differential caught a fragment-length accounting slip (line
count reused as byte count, truncating both bodies); lengths and CRCs are
byte-exact against the next symbol. NULL ops or any NULL callback returns
EINVAL. Construction/rendering internals stay open.

## Still open

Subsequent [SVG-LIST-UPDATE](SVG-LIST-UPDATE-2026-11-03.md) implements the
row-view getters and full refresh. Widget-level row getters and editing,
parent refresh, scrollbar construction/rendering internals,
startSelectAnimation, and the owned backend/context/consumer with
permission/modal/focus/lifetime/visible gates remain open. Bodies are not
actual widget/rendering acceptance, and no visible parity is claimed.

Evidence and reproduction: artifacts/analysis/svg-list-scrollbar-2026-11-03/.
