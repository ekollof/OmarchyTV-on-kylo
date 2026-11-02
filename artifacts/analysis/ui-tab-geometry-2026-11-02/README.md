# Tab bar + title geometry from record 191 (2026-11-02)

`./build/svg-rects` dump of record 191 (the tab bar) from the shipped pack,
cross-checked against the corrected vendor capture:

| element | record rect (1920x1080 space) | capture check |
|---|---|---|
| dark top strip | (0,0,780,162) `#811924` @ opacity 2 over `#BE1E2D` panel | x=400 reads (178,29,43) at y10/50/100 — darker than panel (190,30,45) ✓ |
| tab: picture | (11,104,161,162) | tab row y104..162; white label ink at y~130 ✓ |
| tab: sound | (163,104,313,162) | 150 px slots, 2-3 px gaps ✓ |
| tab: network | (316,104,466,162) | |
| tab: installation | (467,104,617,162) | |
| tab: system | (619,104,769,162) | |
| `dialogTitle` text | (57,5,691,95) | 634x90 area; white title ink present ✓ |
| `glyph` | (38,24,98,84) | 60x60 icon left of title |
| `tab_text` | (0,0,1,1) degenerate | description key, no Dictionary entry (by design) |

Panel red (190,30,45) resumes at y170 = listBox top (record 179). The selected
tab (Beeld) vs unselected backgrounds are NOT decoded — do not invent a
selection treatment.

What this unblocks: title STRINGS (Beeld/Geluid/…, see ui-tab-titles) plus
the title text RECT (57,5,691,95) are now both firmware-sourced. Still open
before live title rendering: the title font ID/size and the text origin
within the rect (needs the textbox layout chain, not a fit), and replacing
baked title pixels without the focus-bar artifact class (opaque baked text
under/over live text). Same rule as rows: decode first, then implement.
