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
- JPEG output is requested RGBA; it is compared against TurboJPEG
  `TJFLAG_FASTDCT`, the mode used by `image.decode`. This is byte-exact on the
  DMR corpus, the two available real foxxum downloads, and a progressive
  variant. A CMYK variant is rejected by both the vendor class and TurboJPEG.
  ImageMagick's default accurate-DCT output differs by up to 8.
- Optional `--extra-jpeg` inputs cover real foxxum files without committing
  vendor assets to the repository.

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
ok vendor jpeg DMR-small.jpg: max channel delta 0, mean 0.000
ok vendor jpeg DMR-large.jpg: max channel delta 0, mean 0.000
ok vendor jpeg variant progressive: max channel delta 0, mean 0.000
ok vendor jpeg variant cmyk: vendor rc=6, TurboJPEG rejects
ok vendor jpeg extra foxxum_1685: max channel delta 0, mean 0.000
ok vendor jpeg extra foxxum_3753: max channel delta 0, mean 0.000
ok vendor decoder differential: 9 PNG + variants, 2 JPEG + progressive/CMYK
```

The PNG result now covers the nine-file corpus plus 1/2/4/8-bit grayscale,
gray+alpha, palette, palette+tRNS, RGB16, RGBA16, interlaced and gAMA variants.
The JPEG result is byte-exact with TurboJPEG `TJFLAG_FASTDCT`. ImageMagick's
default accurate-DCT decoder differs by up to 8; `image.decode` now uses the
fast-DCT mode to match the vendor class. Other JPEG variants (progressive,
CMYK, actual foxxum downloads) remain open.

Identities:

- `aurora.elf` sha256 `7d6e4ca5…`
- probe `2d5aff61…`
- probe source `tests/vendor_decoder_probe.c`; harness
  `tests/vendor_decoder.py`

## Limits

- QEMU executes the vendor classes, but vendor global constructors/UI were not
  run; the class calls are controlled (requested RGBA properties, no-op
  callback).
- Two of the four actual downloaded `foxxum_thumb_*` JPEGs were available in
  the local scratch corpus and are byte-exact; the other two were not copied
  offline. The JPEG corpus otherwise uses DMR-small/large and generated
  progressive/CMYK variants.
- APNG/YCCK behavior, worker scheduling/cancellation, vendor UI
  animation/frame lifetime, on-TV execution and stock pixel parity remain open.
  CMYK is a rejection-policy match on the tested file.
- No TV access or hardware ownership is claimed.
