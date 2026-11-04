# Vendor decoder class differential — 2026-11-04

Offline only. The shipped `PngImageDecoderImpl` and `JpegImageDecoder` classes
were executed under qemu through an ARM LD_PRELOAD probe. The probe runs before
the vendor `main`, so no vendor UI, HAL device, framebuffer or TV is touched.

## Method

`tests/vendor_decoder_probe.c` is loaded with `LD_PRELOAD` into
`aurora.elf` under `qemu-arm-static`. Its constructor:

1. reads the input file;
2. constructs the vendor `MemoryBufferSource`, `MutableImage` and the selected
   decoder class;
3. calls the class `initialize` and `decodeImage` with no requested
   `ImageProperties` and a no-op callback;
4. reads `width`/`height`/`rowlen`/pixels through the vendor `MutableImage`
   vtable;
5. writes `magic, rc, width, height, rowlen, input_size` and the raw rows.

`tests/vendor_decoder.py` runs the probe over the maintained corpus and
normalizes the result:

- PNG non-palette: vendor output is BGRA; swapping R/B must equal the
  ImageMagick/current image.decode RGBA bytes exactly.
- PNG palette: vendor output is one palette-index byte per pixel; indices must
  equal Pillow's indexed source exactly.
- JPEG: vendor output is BGRA; channel deltas against ImageMagick RGBA are
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
ok vendor png launcher_app_thumb336: BGRA->RGBA exact
ok vendor png launcher_app_thumb354: BGRA->RGBA exact
ok vendor png launcher_app_thumb355: BGRA->RGBA exact
ok vendor png launcher_app_thumb356: BGRA->RGBA exact
ok vendor png launcher_app_thumb368: BGRA->RGBA exact
ok vendor png launcher_app_thumb385: BGRA->RGBA exact
ok vendor png launcher_app_thumb476: palette-index exact
ok vendor png launcher_app_thumb495: palette-index exact
ok vendor png launcher_app_thumb61: BGRA->RGBA exact
ok vendor jpeg DMR-small.jpg: max channel delta 8, mean 1.698
ok vendor jpeg DMR-large.jpg: max channel delta 8, mean 1.302
ok vendor decoder differential: 9 PNG, 2 JPEG
```

The PNG result matches the maintained nine-file corpus. The JPEG result shows
the vendor class is not byte-identical to the TurboJPEG path: R/B-normalized
output differs by at most 8 per channel (mean ≈1.3–1.7). That is a real
vendor-class conversion/rounding difference, not a layout or channel-order
defect. A JPEG exactness claim remains open.

Identities:

- `aurora.elf` sha256 `7d6e4ca5…`
- probe `03da65ac…`
- probe source `tests/vendor_decoder_probe.c`; harness `tests/vendor_decoder.py`

## Limits

- QEMU executes the vendor classes, but vendor global constructors/UI were not
  run; the class calls are controlled (no `ImageProperties` request, no-op
  callback).
- The corpus is the nine downloaded PNGs plus two DMR JPEGs. Grayscale/tRNS/
  16-bit/gamma PNG variants and the actual downloaded `foxxum_thumb_*` JPEGs
  are not yet in this differential.
- APNG/CMYK/YCCK behavior, worker scheduling/cancellation, vendor UI
  animation/frame lifetime, on-TV execution and stock pixel parity remain
  open.
- No TV access or hardware ownership is claimed.
