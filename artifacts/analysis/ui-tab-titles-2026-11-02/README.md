# TabDialog title keys → live strings (2026-11-02)

`TabDialog::updateGlyphAndTitle` (0xd6faed) picks the title textbox text per
focused tab from static literal keys (non-Panasonic arms; this set is
Telefunken), falling back to `SETTINGS`:

| tab | key literal (ELF VA) | eng | dut |
|---|---|---|---|
| 0 Beeld | PICTURE (0x14d1b1c) | Picture | Beeld |
| 1 Geluid | SOUND (0x152b8e4) | Sound | Geluid |
| 2 Netwerk | NETWORK (0x13dcdcc) | Network | Netwerk |
| 3 Installatie | INSTALLATION_STR (0x14d8822) | Installation | Installatie |
| 4 Systeem | SYSTEM_STR (0x14e8118) | System | Systeem |
| default | SETTINGS (0x152b68b) | Settings | Instellingen |

Glyph ids per tab: 0x335/0x336/0x337/0x32f/0x338 (0xa2 with the title when
the `tab_glyph` description is present but `tab_text` is not).
Panasonic-customer alternates (PICTURE_MENU/SOUND_MENU/SETUP_MENU) do not
apply here. `updateTitles` (0xd6fcd5) sets the five tab button texts from
localized lookups of five RUNTIME .bss keys (0x1a0f1bc table) — not
statically resolvable; `tab_text` is a description key, not a Dictionary
key, so it has no record by design.

Verification: `verify-title-labels.cjs` traverses all 2435 records × 41
locales of the hash-checked profile
(`mb230_langprofile.bin` MD5 2726ce32..., same file as the MODE proof) with
the firmware hash and asserts every key. Run from the repo root:

```
node artifacts/analysis/ui-tab-titles-2026-11-02/verify-title-labels.cjs
```

This closes the title TEXT source. Title/tab/focused PIXEL origins remain
open (transform/instance inputs); do not render from these strings yet.
Decompiles: /tmp/opencode/decomp-tabs.txt is ephemeral — re-run
DumpFirmware.java @0x00d6fcd5 @0x00d6faed to reproduce.
