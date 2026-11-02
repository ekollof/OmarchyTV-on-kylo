# Settings label source and dictionary — 2026-11-02

Offline firmware investigation resolves the ordinary row's label-content path.
The shipped Dutch translation of MODE is exactly `Modus`, without leading
whitespace or control markup. This rules out padding in that stored translation
as the explanation for conditional X31 versus the old fitted X49. Actual parent,
resource overrides, ratios and live instance state remain unverified. No UI
coordinate, source renderer, TV state or persistent installation changed.

## Label path

Aurora MD5:64c252340d3af6c5d66072611b7bad7a. Fresh read-only Ghidra decompile
and ARM listings are in artifacts/analysis/ui-row-labels-2026-11-02.

- SettingItem key constructor d50018 localizes the supplied key into UString+4.
- Ordinary SettingItemsListBox::drawRow d50f60 copies that UString and calls
  SettingItemsListBoxRow::setLabel; it adds no prefix. Other row types have
  separate value/button/glyph behavior; this is not a claim about all rows.
- UIResourceManager::lookupString114bd24 normally delegates to Dictionary::
  translate11fac40 when the dictionary exists and flag+4c is enabled. The
  alternate translation-fit branch calls lookupLongestString114bc74, which
  chooses a maximum-width translation using the window builder. Its live
  enablement was not read. Do not equate a stored Dutch value with proof of
  every current object's text.
- Dictionary::findString11fabdc binary-searches sorted hashed IDs, not source
  key strings. Dictionary::getStringId11fab4c uses the recovered custom hash,
  not CRC32. The ARM loads signed bytes; the evidence script limits keys to ASCII.
- Dictionary constructor11fa68c reads language/record counts, an8-byte ID/offset
  table, then three NUL-terminated strings per locale, then translation data.
  Runtime shaping/fallback/locale-selection policy is separate from raw decoding.

The existing UI-TEXT-SETUP evidence identifies picture-mode key MODE. The
previously verified setLabel setter, zero margins/font offsets and constructor
insets also supply no fixed18px offset. These exclusions do not prove where
the actual row is placed.

## Profile source and reproducible result

DemoApp::loadDictionary cc542c loads profile `langprofile`, section `Languages`,
value `Dictionary`, with initial language `eng`. This order is established from
the actual r1/r2/r3/stack operands and pointer strings, not argument names alone.
UIResourceManager::loadDictionary114b5f8 obtains the value through ROMProfile.
The `slim_i18n_*` character-conversion symbols are not the key lookup entry point.

The local shipped copy artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin
is5171699 bytes, MD5:2726ce3299039204ef54dbefa43cf33f. ROMProfile::initialize
c5dd44 establishes the root variant at file+30hex and, for this profile's zero
header field+20hex, file-relative data offsets. findSubSection c5e190/findValue
c5e298 walk20-byte entries; section entries skip their descendant counts.
readValue c5e350 confirms length+8/data-offset+c and accepted value types2..4.

| item | verified value |
|---|---|
| Languages section / Dictionary value entry |0x44 /0x38c|
| Dictionary blob / length |0x3dc /5170703 bytes|
| Locale count / hashed records |41 /2435|
| Translation string-data base (file offset) |0x51e8|
| MODE hash / record index / relative string offset |0x5aa5907f /987 /2073999|
| English locale slot0 / value |eng /`Mode`|
| Dutch locale slot10 / value |dut /`Modus`|
| Dutch value including terminator |hex `4d6f64757300`|

Run from the repository root:

```
node artifacts/analysis/ui-row-labels-2026-11-02/verify-label.cjs
```

This offline evidence script checks the exact profile hash, bounded section/
value traversal, locale triples, sorted record IDs and all2435x41 terminated
translations, then the unique MODE record and exact English/Dutch values.
It is not a production localization loader, full encoding/shaping validation
or an independent runtime-rendering test. No C source changed; no build/runtime
or hardware acceptance is claimed. The investigation made no TV connection.

Next recover actual parent/window/resource/ratio state or the relevant coordinate
transform from firmware before replacing fitted positions. A live original
settings object still needs evidence; the heap-only scan's absence is not proof.
Production localization/fallback/shaping and visible FA/UFA parity remain open.
