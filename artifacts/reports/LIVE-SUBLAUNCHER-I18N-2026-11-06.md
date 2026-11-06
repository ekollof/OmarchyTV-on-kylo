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
text, centered in the 235 px card. The label/value pen lines are fixed from the
reference strings; localized ink heights then vary with the font as in the
vendor. Dutch live text measured against the capture:

```text
card 0 label cap (86,19)-(147,37)  live (87,19)-(148,37)
card 1 label cap (34,19)-(198,42)  live (35,19)-(199,42)
card 2 value cap (87,77)-(141,102) live (90,77)-(144,102)
```

All six boxes are within 4 px; most are within 1 px. The QEMU check enforces
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
glyphs measurable. This is **not** BiDi reordering or shaping.

The 41-locale font audit:

```text
ok sub text locales: 41 locales; max label 264 px, max value 183 px;
3 label/0 value overflow, 0 missing glyphs
label overflow: bul ENERGY_SAVING 253 px
label overflow: lit PICTURE_ZOOM 239 px
label overflow: ukr PICTURE_ZOOM 264 px
```

The 235 px live card clips those three long labels symmetrically. The vendor
translation-fit/ellipsis/shrink policy is not yet decoded, and RTL visual order
is still left-to-right; do not claim full all-language visual parity.

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

The final 450 is the 130 px card scaled by 1.35 (175 px). The repeatable check:

```text
ok qemu sub-launcher live: Dutch text bboxes within 4 px;
key 22 timer card bottom 405 -> 450 over 17 frames
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
ok sub text locales: 41 locales; max label 264 px, max value 183 px; 3 label/0 value overflow, 0 missing glyphs
ok qemu sub-launcher live: Dutch text bboxes within 4 px; key 22 timer card bottom 405 -> 450 over 17 frames
full_check=0
```

## Limits

- RTL/BiDi reordering and shaping are not implemented; Hebrew/Arabic strings
  are control-stripped and drawn left-to-right.
- Three long labels exceed the 235 px card and are clipped; the vendor
  translation-fit policy remains undecoded.
- The live values are the p0/03 captured state (`NATURAL`, `MAXIMUM_STR`,
  `ZOOM_MODE_16_9`), not live settings acquisition.
- Font3/font12 identification and text positions are checked against the Dutch
  capture; other locales have string/width evidence, not vendor captures.
- The async quiesce path is tested with a diagnostic delay; production default
  remains a fast synchronous profile free. Only `ir` and `i18n` have the gate.
- No TV run or scanout/Mali parity.
