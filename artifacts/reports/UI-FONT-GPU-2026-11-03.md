# Native GPU glyph pipeline — 2026-11-03

The lower GLES owner now accepts ordered coverage glyphs and reproduces the
shipped GL_ALPHA texture/font shader plus RGBA FBO composition stages. Strict
host, focused sanitizer, ARM/qemu geometry and actual TV offscreen checks pass.
This is a tested rendering primitive, not finished text/UI service integration
or visual parity. The saved title still differs from the stock capture.

## Firmware and live association

Source Aurora ELF MD5:64c252340d3af6c5d66072611b7bad7a. Fresh sequential
read-only/noanalysis Ghidra decompiles AND ARM are preserved in
analysis/ui-font-gpu-2026-11-03. Important entry points:

| address | recovered behavior |
|---|---|
|b34670 drawText|per-glyph coverage texture and clipped UV/quad drawing|
|b343c4 getTextureId / b32eb0 createTexture|GL_ALPHA, unsigned byte, tight unpack1, linear filtering, clamp edges|
|b32e44 convertToOpenGLCoordinate|integer screen half first, then float subtraction/division|
|b33b00 drawTexturedQuad / b33fc4 useFontShader|vec4 position/vec2 UV, 24-byte vertex, integer colour uniform|
|b33428 initialise|blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA for RGB, ONE/ONE for alpha; depth disabled|
|b32ac0 createFrameBufferObject|RGBA byte colour texture, linear filters/clamp, framebuffer attachment|
|b33074 onRenderCompleted|sample the UI FBO into framebuffer0, then vendor swap|

The shipped texture vertex shader has no explicit varying precision qualifier;
both font and texture fragment shaders declare mediump float. Shader pointers
are writable ELF data at178b8bc/178b8c0/178b8c4. The maintained shaders.cjs reads
the correct PT_LOAD mapping and verifies source MD5 before extraction. Do not
apply the first segment's VA/file delta to these pointers.

Guarded view-only reads follow the persistent UIFramework03b81030 through
task03e220f0, client0397d200, Renderer03e81a40, DrawContext03b24f60 and
OpenGLGraphicsContext019f7ff8. The singleton snapshot has dimensions1920x1080,
font program8 and initialized1. This establishes configured renderer association;
it does not trace execution of the settings title command through drawText.
The previously saved Surface06b40840 had already been freed: its new invalid
vptr was rejected, and its apparent renderer field was NOT dereferenced.
Never reuse those heap object addresses as current objects. Private bytes stay
in ignored workbench/title-horizontal-20261103; getter/caller evidence is durable.

## Implementation and scope

common/glyph_quad.c converts absolute top-down glyph bounds, applies optional
clip/UV remapping, and preserves the vendor odd-dimension integer-half behavior.
It does not prematurely clip against the screen: GL clips after interpolation.
Overflow/invalid/fully-clipped cases leave the output unchanged.

oa_scene_gles_render_glyphs validates the whole borrowed batch, then uploads
tight GL_ALPHA masks and draws TL/TR/BR/BL triangle fans with the native shader
arithmetic, colour uniforms and blend factors. It renders the base/glyphs into
an owned RGBA FBO, then samples that texture without flipping its V coordinate.
The composition program retains native varying declarations separately from
the pre-existing generic scene program. The second SRC_ALPHA blend attenuates
translucent RGB again; this is the recovered vendor operation, not ordinary
single-pass premultiplication. Readback storage is bottom-up RGBA; the corpus
writer reverses rows into top-down RGBA. Neither is raw framebuffer BGRA.

All GL calls retain existing same-process/thread/context ownership. New objects
are private and lazy, failures clean partial allocations, and context destruction
releases them. The original cached RGBA API remains usable after glyph calls
and allocation failures. Project limits are4096 glyphs/64MiB coverage per call,
plus actual GL texture limits. The glyph API is synchronous and must remain on
an owning helper/worker, not the host loop or ABI3 quiescence gate.

No glyph wire commands, font-provider holds, UI rendering integration or combined
glyph/effect service were added. ui.title remains default-off. Legacy scene
rendering keeps its existing one-pass contract. Existing production ABI/artifacts
and installed files were not changed.

## Tests and pixel limits

Full strict make check exits0. check-glyph-quad checks exact geometry/UVs,
negative zero, odd dimensions, offscreen bounds, culling, overflow and atomic
errors. Optional check-glyph-gles uses host Mesa surfaceless software rendering;
focused ASan/UBSan passes. It tests pbuffer coverage/orientation, explicit
two-pass alpha/RGB, mask replacement, clip UVs, ordered overlap, whole-batch
rejection, failed FBO texture/font-program/font-texture allocation and legacy
RGBA reuse after failure. Native ARM geometry passes qemu with the glibc sysroot.

The owner explicitly approved the private pbuffer test while idle. The real
Mali-G51 r12p0 test passes synthetic cases and a1920x1080 title corpus using the
live font18 pack, recovered box origin23,5 and surface108,5. No window surface,
swap, framebuffer write, input/HAL initialization or setting change occurs.

Three deliberately distinct raster observations are preserved:

| path | opaque white/core pixels | mask difference from saved vendor |
|---|---:|---:|
|CPU / one-pass native coverage|1971|525|
|two-pass native coverage/FBO|1228|1150|
|saved vendor title crop|1446|reference|

These are equality masks inside583x90 at108,5, not full colour/alpha acceptance.
The first GPU run produces the same opaque core as CPU: changing only to a
glyph shader does not explain the mismatch. Adding the firmware's FBO pass
does not close it either. Replacing generic composition varying declarations
with native declarations gives the SAME full output hash on this driver.
Do not claim a shader-precision fix, adjust placement, or infer bad font bytes
from this result. The exact command route, background/alpha contribution,
framebuffer presentation and actual active rendering state still need evidence.
Native pbuffer success is not independent visible acceptance.

## Identities and cleanup

Isolated build-tv-v3/glyph-gpu/egl-glyph-probe, final tested build:
160268 bytes, MD5 772970e414644b47ec2c3b415a7a1cbd, maximum GLIBC2.7.
The initial one-pass build was154996/318344934455ae41a108e4ac7881c5ca;
the intermediate two-pass build158724/1db3bef80d6463598d1c2e1134a3d293,
native-declaration build160084/9a5345b5e685c919979adc96d385a24c. Final code
also preserves GL errors already consumed by allocation checks during cleanup.
Each was separately size/hash verified before execution, then removed.
Matching scene-renderer.so was built, not deployed or loaded by a service.

Final corpus8294400 bytes MD5 25e09914c06a252c31cf92de49d419ef, downloaded
hash verified, then removed from /tmp. The intermediate composition gives
the same hash; one-pass output is118b821e7d36066074eca89dbfa3d34d.
Final executable scan and independent file checks show no helper/staging.
Boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, fb1 pan0,1080 and full backing MD5
41735daa69dc137743b59efe12db4a19 are unchanged before/after all tests. Hook
0b1cd32a9de2b0251f6252e6603db8b7 and hosts b6eee64775233de846ca46fc6c48c3f9
unchanged; SSH22/telnet2323 listening and Samba OUTPUT REJECT remain present.
Two intermediate postchecks used nonexistent iptables paths; the final query
uses the actual /vendor/iptables and succeeds. That command lookup error is
not loss of firewall rules. No service restart, reboot or persistent deployment.
