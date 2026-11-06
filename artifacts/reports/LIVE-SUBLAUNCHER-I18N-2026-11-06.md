# Live sub-launcher text, runtime animation and i18n quiesce — 2026-11-06

TV was off limits. Everything below is offline: the real ARM UI stack under
`qemu-arm-static`, the shipped `mb230_langprofile.bin`, and the existing
p0/03 oracle capture. No TV, fbdev, input device or stock process was touched.

## Live text

The p0/03 card fonts were identified by matching rendered ink boxes against
the vendor capture:

- label: font **3** (`Modus` 63x18 vs capture 64x19; `Energiebesparing`
  166x23 vs 167x24; `Beeld zoomen` 138x18 vs 139x19);
- value: font **12** (`Natuurlijk` 133x33 vs 134x34; `Maximaal` 129x26 vs
  130x27; `16:9` 54x25 vs 55x26).

`ui_vendor` now renders `SCREEN_SUBLAUNCHER` from the JSON picture branch with
clean captured card backgrounds (`sublauncher_card_N_bg`) and live label/value
text. The text surface is the vendor row box: `SettingItemsListBoxRowItems_sublauncher`
`text1` is **240x54** (rectangles.tsv hash `0x0db5c59b`, offset 10,-134), so the
text canvas is 240 px wide and is blitted at the checked runtime item origin
(`runtime_item_x` = 199), not the 235 px card inset. The label/value pen lines
are fixed from the reference strings; localized ink heights then vary with the
font as in the vendor. Dutch live text measured against the capture:

```text
card 0 label cap (86,19)-(147,37)  live (87,19)-(148,37)
card 0 value cap (52,76)-(184,109) live (51,76)-(183,109)
card 1 label cap (34,19)-(198,42)  live (36,19)-(200,42)
card 1 value cap (52,76)-(181,102) live (53,76)-(182,102)
card 2 label cap (48,19)-(184,37)  live (50,19)-(186,37)
card 2 value cap (87,77)-(141,102) live (91,77)-(145,102)
```

All six boxes are within 4 px; five are within 2 px. The QEMU check enforces
that tolerance. This is a live text path, not the baked cutout.

## All interface languages

`common/langprofile.c` decodes the EFSL `langprofile` `Dictionary` and the
verified custom hash. `mod_i18n` publishes `i18n` v1 with all **41 locales**.
The lookup follows `Dictionary::translate` exactly:

```text
current locale -> "eng" -> the key itself
```

`UIResourceManager::lookupString` (0x114bd24) delegates to that path; the
fallback language pointer at 0x11face4 is `"eng"`. Dutch `ZOOM_MODE_16_9` is
empty, so the vendor's “16:9” comes from the English fallback; the test checks
this exact case.

`tools/re/langprofile-dump.cjs` dumps every locale for a key. The live text
uses keyed labels (`MODE`, `ENERGY_SAVING`, `PICTURE_ZOOM`) and value keys
(`NATURAL`, `MAXIMUM_STR`, `ZOOM_MODE_16_9`), not Dutch constants.

`oa_langprofile_sanitize` strips Unicode directional formatting controls
(U+200E/200F, U+202A..202E, U+2066..2069) that the font cannot draw. The Hebrew
`ENERGY_SAVING` string carries U+202C; stripping it makes the existing Hebrew
glyphs measurable. The sanitizer is not BiDi reordering or shaping.

## Vendor fit and RTL policy

`SettingItemsListBoxRow::renderItems` (0x00d54400) does not shrink or measure
the translation for the card. It sets:

- the label `SVGTextBox` to font `0x1a` and `SVGTextBox::setElipses(false)`;
- the value `SVGButton` to font `0x1e`, `SVGButton::setElipses(true)` and text
  alignment 4.

There is no width-based font selection in the row path. `UIFramework`'s
translation-fit check is only a diagnostic chord from
`DemoApp::preHandleKeyPressMessage` (0xcff854), not production layout. So at
the 240 px row box the label policy is **clip**, and the value policy is
**elide** (no value overflows in this branch).

The profile's Hebrew values are already stored in **visual order**: `MODE` is
`'בצמ'` (the display form of `'מצב'`), `ENERGY_SAVING` is `'למשחב ןוכסח'`
(`'חסכון בחשמל'`), `NATURAL` is `'יעבט'` (`'טבעי'`). `Dictionary` runs
`ArabicShaping::shapeString(...,true)`, which converts the stored visual form
to logical order for the vendor's RTL text engine; that engine lays logical
text out right-to-left. Because `ui_vendor` draws with a plain LTR bitmap
renderer, it must use the stored visual order directly and **must not reverse
it again**. A QEMU Hebrew frame renders `מצב`, `חסכון בחשמל`,
`קירוב והרחקה של תמונה`, `טבעי`, `מקסימום` and LTR `16:9`.

Arabic/Persian values are different: they are stored logical and already
carry Arabic presentation forms (`MODE` `'ﻭﺿﻊ'`). `common/rtl_order.c`
converts them to visual order for the LTR renderer: Arabic code points are
RTL, Latin/digits are LTR, neutral runs (space/punctuation) take the RTL
paragraph direction, the run order is reversed, and each Arabic run is
reversed. A string with no Arabic code point is copied unchanged, so the
English `16:9` fallback stays LTR. Bracket mirroring across runs is not
implemented. QEMU Arabic renders `وضع`, `توفير الطاقة`,
`تكبير/تصغير الصورة`, `طبيعي`, `حد أقصى`, `16:9`; Persian renders `حالت`,
`صرفه جویی در مصرف انرژی`, `ابعاد تصویر`, `طبیعی`, `حداکثر`, `16:9`.

The 41-locale font audit against the vendor 240 px box:

```text
ok sub text locales: 41 locales; max label 264 px, max value 183 px;
2 label/0 value overflow, 0 missing glyphs
label overflow: bul ENERGY_SAVING 253 px
label overflow: ukr PICTURE_ZOOM 264 px
```

Lithuanian `PICTURE_ZOOM` (239 px) now fits. Bulgarian and Ukrainian clip
symmetrically in the 240 px text surface, matching `setElipses(false)`. Values
max 183 px, so the vendor value ellipsis path is not exercised.

## Runtime 300 ms animation

`SCREEN_SUBLAUNCHER` now handles Left/Right (key ids 21/22): the selected card
index changes, `sub_anim_start` is taken from `ui.clock`, and a 10 ms timer
repaints until elapsed ≥ 300 ms. `paint_sub_launcher` uses the elapsed time (or
the `ui.sub_progress` lab value when not live) for the card scale.

The QEMU stack loads `plat` (no-vendor stub), `ir` with no UART and BLAST
disabled, `ui_engine`, `ui_menu`, `i18n`, `ui_clock`/`ticker` and `ui_vendor`.
The existing control `key 22` command feeds the real IR parser; the parser
emits the key event. Captured card 0 bottom edge:

```text
static 405; frames grow 407, 411, 415, 420, 424, 428, 432, 436, 442, 447, 450
```

The final 450 is the 130 px card scaled by 1.35 (175 px). The repeatable check
accepts `OA_QEMU_LANG`; Dutch compares against p0/03, other locales assert the
text stays inside the 240 px box and the same key/timer path runs:

```text
ok qemu sub-launcher live: Dutch text within 4 px of p0/03;
key 22 timer card bottom 405 -> 450 over 14 frames
ok qemu sub-launcher live: heb text inside the 240 px box;
key 22 timer card bottom 405 -> 450 over 27 frames
```

## Asynchronous i18n quiesce

`mod_i18n` now uses the ABI 3 quiesce gate. It unpublishes the service,
detaches the profile, and frees the 5 MB profile on a worker (diagnostic
`i18n.cleanup_delay_ms`; default fast synchronous free). While the worker is
busy, `reload i18n` fails fast and the control loop stays responsive; a retry
completes. Test:

```text
ok i18n quiesce: reload fails fast, stays responsive, retry completes
```

This is the second module with an asynchronous quiesce gate after `mod_ir`.
Most other modules still stop synchronously.

## Verification

```text
ok langprofile: 41 locales, localized MODE/ENERGY_SAVING/PICTURE_ZOOM and eng->key fallback
ok i18n service: 41 locales, Dutch/English reload and failed-load isolation
ok i18n quiesce: reload fails fast, stays responsive, retry completes
ok sub text locales: 41 locales; max label 264 px, max value 183 px; 2 label/0 value overflow, 0 missing glyphs
ok rtl order: Arabic/Persian run visual order and LTR fallbacks
ok qemu sub-launcher live: Dutch text within 4 px of p0/03; key 22 timer card bottom 405 -> 450 over 14 frames
ok qemu sub-launcher live: heb text inside the 240 px box; key 22 timer card bottom 405 -> 450 over 27 frames
ok qemu sub-launcher live: ara text inside the 240 px box; key 22 timer card bottom 405 -> 450 over 28 frames
ok qemu sub-launcher live: per text inside the 240 px box; key 22 timer card bottom 405 -> 450 over 14 frames
full_check=0
```

## Limits

- Hebrew uses the stored visual order. Arabic/Persian now use the run reorder
  above, but bracket mirroring across runs and mixed Hebrew/Latin BiDi are not
  implemented. The Arabic/Persian order is a renderer-side transform verified
  by QEMU rendering, not a capture-compared vendor oracle.
- Bulgarian and Ukrainian labels exceed the 240 px row box and clip; the vendor
  value ellipsis path is decoded only from `setElipses(true)`, with no
  overflowing value to exercise it.
- The live values are the p0/03 captured state (`NATURAL`, `MAXIMUM_STR`,
  `ZOOM_MODE_16_9`), not live settings acquisition.
- Font3/font12 identification and text positions are checked against the Dutch
  capture; other locales have string/width evidence, not vendor captures.
- The async quiesce path is tested with a diagnostic delay; production default
  remains a fast synchronous profile free. Only `ir` and `i18n` have the gate.
- No TV run or scanout/Mali parity.
