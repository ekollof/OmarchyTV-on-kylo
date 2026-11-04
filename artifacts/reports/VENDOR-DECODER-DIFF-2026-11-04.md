# Vendor decoder class differential — 2026-11-04

Offline only. The shipped `PngImageDecoderImpl` and `JpegImageDecoder` classes
were executed under qemu through an ARM LD_PRELOAD probe. The probe runs before
the vendor `main`, so no vendor UI, HAL device, framebuffer or TV is touched.

## Method

`tests/vendor_decoder_probe.c` is loaded with `LD_PRELOAD` into
`aurora.elf` under `qemu-arm-static`. Its constructor:

1. reads the input file;
2. constructs the vendor `MemoryBufferSource`, `MutableImage`, requested
   `ImageProperties`/`ColourModel` and the selected decoder class;
3. calls the class `initialize` and `decodeImage` with requested RGBA
   properties (model 3) and a no-op callback;
4. reads `width`/`height`/`rowlen`/pixels through the vendor `MutableImage`
   vtable;
5. writes `rc, width, height, input_size, rowlen` and the raw rows.

`tests/vendor_decoder.py` runs the probe over the maintained corpus and
generated PNG format variants:

- PNG output is requested RGBA; it must equal the source RGBA bytes exactly.
  (Without requested properties the class produces BGRA; model 2 requests
  BGRA and model 3 requests RGBA.)
- JPEG output is requested RGBA; channel deltas against ImageMagick RGBA are
  measured. The current DMR corpus requires max delta ≤8 (codec rounding), not
  byte-exact parity.

Command:

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/vendor-decoder \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  check-vendor-decoder
```

## Results

```
ok vendor png launcher_app_thumb336: RGBA exact
ok vendor png launcher_app_thumb354: RGBA exact
ok vendor png launcher_app_thumb355: RGBA exact
ok vendor png launcher_app_thumb356: RGBA exact
ok vendor png launcher_app_thumb368: RGBA exact
ok vendor png launcher_app_thumb385: RGBA exact
ok vendor png launcher_app_thumb476: RGBA exact
ok vendor png launcher_app_thumb495: RGBA exact
ok vendor png launcher_app_thumb61: RGBA exact
ok vendor png variant gray1: RGBA exact
ok vendor png variant gray2: RGBA exact
ok vendor png variant gray4: RGBA exact
ok vendor png variant gray8: RGBA exact
ok vendor png variant graya8: RGBA exact
ok vendor png variant palette: RGBA exact
ok vendor png variant palette_trns: RGBA exact
ok vendor png variant rgb16: RGBA exact
ok vendor png variant rgba16: RGBA exact
ok vendor png variant interlaced: RGBA exact
ok vendor png variant gamma: RGBA exact
ok vendor jpeg DMR-small.jpg: max channel delta 8, mean 1.698
ok vendor jpeg DMR-large.jpg: max channel delta 8, mean 1.302
ok vendor decoder differential: 9 PNG + variants, 2 JPEG
```

The PNG result now covers the nine-file corpus plus 1/2/4/8-bit grayscale,
gray+alpha, palette, palette+tRNS, RGB16, RGBA16, interlaced and gAMA variants.
The JPEG result remains a real class conversion/rounding difference: after
requested-RGBA normalization it differs by at most 8 per channel (mean
≈1.3–1.7). An exact JPEG-class claim remains open.

Identities:

- `aurora.elf` sha256 `7d6e4ca5…`
- probe `2d5aff61…`
- probe source `tests/vendor_decoder_probe.c`; harness
  `tests/vendor_decoder.py`

## Limits

- QEMU executes the vendor classes, but vendor global constructors/UI were not
  run; the class calls are controlled (requested RGBA properties, no-op
  callback).
- The actual downloaded `foxxum_thumb_*` JPEGs are not offline. The JPEG corpus
  uses DMR-small/large.
- APNG/CMYK/YCCK behavior, worker scheduling/cancellation, vendor UI
  animation/frame lifetime, on-TV execution and stock pixel parity remain open.
- No TV access or hardware ownership is claimed.
