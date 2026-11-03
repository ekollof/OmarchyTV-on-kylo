# GPU coverage/FBO evidence

Interpretation and scope: ../../reports/UI-FONT-GPU-2026-11-03.md.
Fresh DumpFirmware.java --asm logs are read-only/noanalysis against Aurora
MD5 64c252340d3af6c5d66072611b7bad7a. Shader extraction:

```
node artifacts/analysis/ui-font-gpu-2026-11-03/shaders.cjs
```

font-texture-draw, coordinate-conversion and final-composition contain the
actual ARM/API evidence, not guessed Ghidra argument counts. Other firmware
logs establish the guarded live framework->renderer->context association.
The old title Surface had been freed; its rejected contents are not a new
renderer pointer. Raw process reads, fonts and raster outputs stay local under
workbench/title-horizontal-20261103, never committed.

Native test groups: tv-* without qualifier=one-pass coverage; tv-two-pass-*=FBO
composition with existing scene shader; tv-exact-*=separate composition shader
with shipped varying declarations. Both FBO variants give the same full hash.
tv-final-* repeats the native checks after the allocation-error diagnostic fix;
its corpus has the same hash. None of these stages presents a window.
tv-final-cleanup confirms absent staging/helper and actual /vendor/iptables
rules; prior failed executable lookup in postchecks is not firewall loss.

Host optional Mesa check exercises actual GLES, including failed allocation
recovery. Host full/sanitizer/ARM build logs are archived here. Qemu tests only
pure glyph geometry; it does not emulate Mali. The real native tests are private
pbuffers, not window/scanout tests; title parity and service integration are open.

Saved core comparison (local inputs only):

```
magick -size 1920x1080 -depth 8 rgba:artifacts/workbench/title-horizontal-20261103/gpu-title-native-two-pass.rgba -crop 583x90+108+5 +repage -alpha off -fx 'r==1&&g==1&&b==1?1:0' /tmp/oa-gpu-core.png
magick /tmp/oa-gpu-core.png -format '%[fx:mean*w*h]\n' info:
magick compare -metric AE /tmp/oa-gpu-core.png artifacts/workbench/title-horizontal-20261103/title-core-vendor.png null:
```

Output1228 and1150 differing pixels (compare exit1 is expected). Vendor1446,
one-pass1971/525. These are narrow equality masks, not full colour acceptance.
