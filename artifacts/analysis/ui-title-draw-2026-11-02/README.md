# Title draw chain investigation, completed2026-11-03

Source ELF MD5 64c252340d3af6c5d66072611b7bad7a; fresh Ghidra sessions all used
-readOnly -noanalysis -process aurora.elf, sequentially, DumpFirmware.java --asm:

| output | addresses |
|---|---|
|foreground-renderer.txt|@10c7a00 @1133050|
|horizontal-controls.txt|@1134790 @1134874 @b442bc|
|line-initialization.txt|@1132ab8 @11331b8 @1133210 @10c7828|
|text-processing.txt|@11333e0 @11338dc @1132f64|

Interpretation/verification scope: ../../reports/UI-TITLE-BOX-2026-11-03.md.
Saved Ghidra prototypes are metadata; ARM register/stack values take precedence.
The local5px center offset follows font18 logical height80 and surface height90.
No universal horizontal23px offset has been derived or implemented.

Host full/sanitizer logs and raw native staging/verify/test/cleanup reads are
preserved here. Isolated ARM output: openaurora/build-tv-v3/title-box. Qemu used
the workbench gcc-arm-10.2-2020.11 ARM glibc sysroot, tests/font_box with shipped
SVG/font pack arguments and ui_settings_text with modes on/missing/notitle/
badtitle/preset/editoff over the test recorder DSOs. Those UI tests do not draw
on physical hardware. Only font_box was staged/executed on the actual TV;
it renders to private process memory and reads resource files. All staging gone.
