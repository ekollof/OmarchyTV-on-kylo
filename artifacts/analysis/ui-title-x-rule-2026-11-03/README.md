# Signed global X offset — evidence and checks

Interpretation: ../../reports/FONT-X-OFFSET-2026-11-03.md. This corrects the
earlier zero-getter error: b4247c loads signed font+5, b42482 negates it.

font-origin.txt: DumpFirmware.java --asm @b42468 @b428d4 @1132ab8 @1134790,
Aurora project, -readOnly -noanalysis, ELF MD5
64c252340d3af6c5d66072611b7bad7a. The earlier mixed rect-and-style query
contained a valid URect constructor followed by invalid addresses; partial
log archived under ignored workbench/title-horizontal-20261103, not an API
finding or a queue of queries to repeat.

live-font-read.txt records the boot/executable-guarded9-byte read; private font
header in workbench/title-horizontal-20261103/title-font-header.bin. The corrected
../ui-title-horizontal-2026-11-03/verify-graph.cjs checks it alongside that
saved graph. Live header e9 and shipped extrema-23 agree independently.

Host/sanitizer/corpus/ARM build logs and native upload/verify/test/cleanup are
preserved here. qemu used the durable gcc-arm-10.2 ARM glibc sysroot with
build-tv-v3/font-x tests/font_box (actual SVG/font files) and ui_settings_text
modes on/missing/notitle/badtitle/preset/editoff over recorder DSOs. Only the
memory-only font_box corpus was executed on the TV, not a display test.

Optional local raster export:

```
openaurora/build/tests/font_box SVG_INDEX SVG_BINARY FONT_INDEX FONT_BINARY OUTPUT.oai
```

Files under workbench/title-horizontal-20261103 include title-corrected.oai,
its RGBA payload and title-core-ours/vendor.png. Core extraction used alpha
equal1 for our raster and RGB all equal1 for vendor crop583x90+108+5. The
earlier threshold99.999% attempt quantized to the maximum and made an empty
mask; it was discarded. The actual equality masks differ by525 pixels.
That comparison is not full colour/antialiasing acceptance; no new offsets
were fitted to it and title remains default-off.
