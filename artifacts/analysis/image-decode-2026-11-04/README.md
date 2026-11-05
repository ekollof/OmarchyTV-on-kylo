# image.decode evidence

Interpretation: [IMAGE-DECODE](../../reports/IMAGE-DECODE-2026-11-04.md).

Nine real files from `artifacts/workbench/thumbs/` (RGB, RGBA and colormap PNGs)
and two real vendor JPEGs from the DLNA/DMR dump are decoded by
`mod_image_decode`. PNG is compared with ImageMagick/libpng; JPEG uses
TurboJPEG `TJFLAG_FASTDCT`, which matches the shipped vendor JPEG class
exactly (ImageMagick's accurate DCT differs by up to8).

- `host-image-decode.txt`: final `check-image-decode` result, including the
  non-PNG, truncated PNG/JPEG, suspend/resume and reload cases.
- `sanitizer.txt`: focused ASan/UBSan PNG corpus result (clean).
- `sanitizer-jpeg.txt`: focused ASan/UBSan PNG+JPEG decoder result (clean).
- `ui-jpeg-sanitizer.txt`: ASan/UBSan `check-ui-jpeg-cards` (clean).
- `arm-qemu.txt`: PNG corpus under ARM/qemu with vendor libpng/zlib.
- `arm-qemu-jpeg.txt`: PNG+JPEG corpus under ARM/qemu with vendor libpng/zlib
  and vendor libjpeg-turbo 2.0.1.
- `ui-arm-qemu.txt`: `ui_live_cards ... png` under ARM/qemu; nine cards use
  decoded 400x225 sources.
- `ui-jpeg-arm-qemu.txt`: `ui_live_cards ... jpeg` under ARM/qemu; two cards
  use decoded 48/120 px sources.
- `ui-png-cards.txt`: host `check-ui-png-cards`; `pngcards.png` is the
  flattened recorder raster with the nine real app images.
- `ui-jpeg-cards.txt`: host `check-ui-jpeg-cards`; `jpegcards.png` is the
  flattened raster with the two real JPEG cards.
- `reference-hashes.txt`: independent ImageMagick PNG RGBA values.
  `jpeg-reference-hashes.txt`: fast-DCT/vendor-class SHA256/FNV-1a values.
- `full-make-check.txt`, `full-make-check-jpeg.txt`: full strict host
  `make check` logs (344 at the original PNG step; current 395).

```sh
make -C openaurora check-image-decode check-ui-png-cards check-ui-jpeg-cards
qemu-arm-static -L .../libc openaurora/build-tv-v3/image-review/tests/image_decode \
  .../modules artifacts/workbench/thumbs artifacts/dumps/apdat_files/dlna-dmr/xml/icon
```

The TV was not accessed after the owner's stop instruction. This is offline
codec and recorder-layout evidence, not a vendor `PngImageDecoderImpl` /
`JpegImageDecoder` differential, on-TV decode, or stock pixel parity.
