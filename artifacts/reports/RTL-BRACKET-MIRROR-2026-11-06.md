# RTL paired-punctuation mirroring — 2026-11-06

Offline UI text parity fix for the Arabic/Persian LTR-renderer workaround.
No TV, display or capture was used.

## Contract

The shipped EFSL `ara`/`per` values are logical-order Arabic presentation forms.
The vendor feeds them through its RTL text engine; this project draws with an
LTR bitmap font and therefore converts each string to visual order in
`common/rtl_order.c`. The helper already reversed RTL runs and kept Latin runs
internally LTR, but explicitly did **not** mirror paired punctuation.

Unicode UAX #9 rule L4 says a character with `Bidi_Mirrored=Yes` is drawn with
its mirrored glyph when its resolved direction is RTL. In a bracketed Arabic
word the run reversal would otherwise swap `(` and `)` without exchanging the
glyphs, producing `)ب(` instead of `(ب)`.

## Implementation

- `mirror_cp()` maps the paired punctuation needed by the profile text:
  `() [] {} <> «» ‹› ⁅⁆ ⁽⁾ ₍₎ ≤≥ ≦≧`.
- Mirroring is applied only while emitting a run whose resolved direction is
  RTL. LTR runs and the early no-Arabic fallback are unchanged.
- No Hebrew change: those profile values are stored visual and bypass this
  helper.

## Verification

`check-rtl-order` adds hand-checkable bracket cases plus the real 41-locale
profile corpus:

```text
ok rtl order: Arabic/Persian run visual order and LTR fallbacks
```

New assertions:

```text
(ب)          -> (ب)            (was )ب( without mirroring)
ABC (ب)      -> (ب) ABC
[(ب)]        -> [(ب)]
(abc)        -> (abc)          (no Arabic, early LTR fallback)
```

Focused ASan+UBSan passes with the real `vendor2/mb230_langprofile.bin`.
Full suite ran visibly and ended `all tests passed`; QEMU sub-launcher
value/live/launcher checks pass, including the live Arabic/Persian rendering
fixtures.

## Limits

- This is the standard BiDi mirroring rule, not an instruction-level recovery of
  the vendor text engine; no vendor frame/capture confirms its exact bracket
  glyph set. Only common paired punctuation is mapped.
- Mixed Hebrew/Latin BiDi (Hebrew is stored visual in this profile) and
  bracket-pair resolution across mixed-direction runs remain open.
- No visible-panel acceptance; broad UI/text parity still requires captures and
  the owner-authorized display session.
