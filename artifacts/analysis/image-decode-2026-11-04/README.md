# image.decode evidence

Interpretation: [IMAGE-DECODE](../../reports/IMAGE-DECODE-2026-11-04.md).

Nine real files from `artifacts/workbench/thumbs/` (RGB, RGBA and colormap PNGs)
are decoded by `mod_image_decode` and compared with independent
ImageMagick/libpng RGBA output.

- `host-image-decode.txt`: `check-image-decode` result.
- `sanitizer.txt`: focused ASan/UBSan build/result (clean).
- `arm-qemu.txt`: the same decoder unit under ARM/qemu using the vendor
  `libpng16.so.16`/`libz` files.
- `ui-arm-qemu.txt`: `ui_live_cards ... png` under ARM/qemu; nine cards use the
  decoded 400x225 sources.
- `ui-png-cards.txt`: host `check-ui-png-cards` result; `pngcards.png` is the
  flattened 1920x1080 recorder raster with the nine real app images.
- `reference-hashes.txt`: independent ImageMagick RGBA SHA256 and FNV-1a values
  for each source file.
- `full-make-check.txt`: full strict host `make check` (344 ok lines).

```sh
make -C openaurora check-image-decode check-ui-png-cards
qemu-arm-static -L .../libc openaurora/build-tv-v3/image-review/tests/image_decode \
  openaurora/build-tv-v3/image-review/modules artifacts/workbench/thumbs
```

The TV was not accessed for this step. This is offline/libpng output and
recorder layout evidence, not vendor `PngImageDecoderImpl` parity, on-TV
decoding, JPEG support or stock pixel parity.
