# Sub-launcher text fit, shaping and RTL — 2026-11-06

TV-offline Ghidra/objdump evidence for the vendor row text policy. Program:
`aurora.elf` MD5 `64c252340d3af6c5d66072611b7bad7a` (Ghidra project
`artifacts/workbench/ghidra_proj`, read-only, no analysis). Decompiler output is
analysis metadata; the citations below are the verified instructions/functions.

| file | scope |
|---|---|
| `fit-decompile.txt` | `UIWindowBuilder::calculateMaximumTextWidthIndex` (0x11532b0), `calculateTextWidth` (0x115318c), `UIFramework` fit toggle (0x113b6e8/0x113b718), `SVGTextBox::calculateTextWidth` |
| `row-render-decompile.txt` | `SettingItemsListBoxRow::renderItems` (0x00d54400) plus `setLabel`/`setTextValue`/`setButtonText`/`getText` |
| `sublauncher-text-decompile.txt` | `SubLauncherDialog::updateTexts`/`updateSettingsList` |
| `textbox-draw-decompile.txt` | `SVGTextBox::renderForeground` (0x010c7a00) and `drawTextBox` |
| `svg-drawtext.txt`, `uisurface-drawtext.txt`, `uisurfaceimpl-drawtext.txt` | `SVGSurface`/`UISurface`/`UISurfaceImpl::drawText`; `param_11` selects `hal::direction` 3 and RTL justification |
| `shaping-decompile.txt` | `Dictionary::Dictionary` (0x011fa68c), `ArabicShaping::shapeString` (0x00c89664), `RightAlignedLanguageTextReverser::reverse`/`reverseString`, `getShapedUstring` |
| `shape-arabic-decompile.txt` | `shapeArabic` (0x00c88904), `reOrderLine` (0x00c88d60), `copyString`/`copyStringReverse`, `isArabic`/`isHebrew`, `isPunctuationMark`, `ArabicShaping::reverseLine` |
| `reorder-helpers.txt` | `getWordLen` (0x00c88cc0), `needMirror` (0x00c88d04) |
| `render-foreground.txt`, `replace-numbers.txt` | exploratory checks; `KeyboardTextEntryBox` only, and `UISurface::replaceNumbers` is digit substitution, not reversal |

Verified findings:

- `SettingItemsListBoxRow::renderItems` calls `SVGTextBox::setElipses(false)`
  for the label and `SVGButton::setElipses(true)` for the value. It does not
  call `calculateTextWidth` or choose a smaller font, so there is no
  shrink-to-fit in the card row.
- The row label text box is `text1` 240x54 (hash `0x0db5c59b`, rect 10,-134).
  The card is 235x130; the two are distinct vendor rectangles.
- `Dictionary::Dictionary` at 0x11fa68c compares the locale to `ara`
  (0x11fa988), `per` (0x11fa98c) and `heb` (0x11fa990), then calls
  `ArabicShaping::shapeString` with `false` for ara/per and `true` for heb.
- The profile's Hebrew values are stored in visual order (`MODE` `'בצמ'`,
  `SETTINGS` `'תורדגה'`); Dictionary converts them to logical order for the
  vendor RTL engine. Arabic/Persian values are stored logical and already carry
  presentation forms (`MODE` `'ﻭﺿﻊ'`).
- The translation-fit check really is diagnostic: the only caller of
  `setTranslationFitCheckOptionEnabled` is `DemoApp::preHandleKeyPressMessage`
  at 0xcff854.
