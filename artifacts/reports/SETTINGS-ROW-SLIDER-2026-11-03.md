# Settings-row renderItems type 0x9 (slider row) — 2026-11-03

`common/settings_row_render.c` now covers type 0x9, the slider row. It looks
up **"slidebar"** then **"text2"** (that order), drives the row+0x1ca8 text box
and the row+0x13c0 slide bar. The text box is created with the char* empty
label and extra 1 (or refreshed via setWindowRect) and always receives the
formatted row[0x2df8] value; uncreated texts use the 0x26d names plus font
0x1e when key1 is present, else 0x1e8/0x1e9/0x1ea/0x1eb. The slider is created
with row[0x2df0]/row[0x2df4] as range and then setSliderValue from
row[0x2df8]; uncreated sliders use 0x26c names and setFocus(0). **95624**
native/C comparisons pass with exact call order/args, rect words, text bytes
and row state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior
groups and a focused real-TV CPU run (exit 0).

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. Jump
table entry type 9 -> 0xd5556a.

Key details the differential pinned down:

- The two rectangle keys are **"slidebar"** (0xd55754) and **"text2"**
  (0xd55758), looked up in that order; the text box consumes "text2" and the
  slider consumes "slidebar". My first pass had the keys and the parameter
  order reversed.
- The text box uses `SVGTextBox::create(int, char const*, EASIRect const&,
  int, UIWindow*)` with the empty char* label at 0xd55750 — a different
  overload from the UString-based creates.
- `SVGTextBox::setText` after `UString::operator<<(UString&,int)` receives the
  `%d`-formatted number; the C side emits the same text through `set_text`.
- The row words are 0x2df0/0x2df4 (range) and 0x2df8 (value), all outside the
  earlier 0x2e8c window.

## Verification and limits

**95624** native/C comparisons: 16 types × 128 eight/eight-key presence masks
× created × 3 point variants × row-word set × label pair × rebuild flag,
minus the type-4 / type-5-6 / type-7 firmware-crashing inputs, plus eight
default-type cases. Host units add the slidebar/text2 lookup order, the
name sets, font 0x1e, the empty-label textbox create, the formatted value and
the slider range/value. Strict full host/actual Unix IPC, focused ASan/UBSan,
fresh ARM/qemu units, every prior group and the focused TV CPU test pass.
Isolated build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 136416 | 12625ef29fda096965cb78d1c58d5b97 |
| tests/tab_policy_firmware | 1112368 | efe772995d71a2f1f264f0ec8694b2a2 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake and verified the no-reset flag;
staging was hash-checked before execution, the test exited 0 and staging was
removed with no helper left. No vendor fragment, device API, screen/input/
audio control, service restart, network/settings/boot mutation or persistent
deployment was run on the TV.

Also corrected this turn: types 5/6's rebuild fold reads the **text1**
rectangle (slot 12), not image1, and `setScalingType` receives the stale
isCreated result while only `create` gets the 0x92/0x93 row word.

Still open: case bodies 0xa..0xc, 0x11..0x15 and 0x17..0x19, getImageId's real
62-way mapping, the image/selection internals, row construction, child
visibility internals, live resources, scrollbar internals, skin/module/
display/scanout/startup and independent visible parity.
