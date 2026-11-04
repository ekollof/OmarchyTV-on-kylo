# Downloaded PNG decoding — 2026-11-04

`image_decode` is a reloadable module publishing `image.decode` v1. It decodes
the vendor-downloaded PNG assets in `/buffer/downloaded_images` into straight
RGBA, and `ui_engine` falls back to it when `<ui.dir>/<name>.rgba` is absent.
Nine real downloaded files — RGB, RGBA and colormap PNGs — decode byte-exactly
to an independent libpng/ImageMagick reference and render through the UI
recorder. No TV access or hardware change was made in this step.

## Firmware basis

`aurora.elf` contains `hal::PngImageDecoderImpl` (`0xb2f6a0` region, including
`createDecodingStructures`, `readHeader`, `decodeImage`, `isPng`) and a
statically linked `cabot_png_*` libpng. The shipped rootfs also carries
`/vendor/lib/libs/libpng16.so.16` (libpng 1.6.28, `PNG16_0` ABI) and
`libz.so`. The earlier read-only TV listing of `/buffer/downloaded_images`
showed extensionless PNG and JPEG files; offline copies of nine PNGs are in
`artifacts/workbench/thumbs/`.

The module uses libpng's simplified file API (`png_image_begin_read_from_file`,
`png_image_finish_read`, `png_image_free`) through `dlopen`, with the vendor
absolute path fallback and a preloaded vendor `libz.so`. It does not call the
vendor `PngImageDecoderImpl`, so this is the same codec library and output
contract, not a differential against that vendor class.

## Implementation

- `modules/image_decode/image_decode.c`: `image.decode` v1, straight top-down
  RGBA, 8192-pixel dimension bound, overflow-checked `w*h*4`, invalid file/
  error rejection, and no retained file or hardware state. Stop/start reloads
  the library and republishes the service.
- `ui_engine`: OAI1 `<ui.dir>/<name>.rgba` remains first; on a miss it looks
  up `image.decode`, decodes `<ui.download_dir>/<name>` (default
  `/buffer/downloaded_images`), caches the RGBA icon and releases the service
  hold. Missing decoder or bad file leaves the icon absent, as before.
- `tools/boot/openaurora.conf` autoloads `image_decode` before `ui_engine`.

## Verification

- `check-image-decode`: nine real files (400x225) match independent
  ImageMagick/libpng RGBA SHA256 and FNV-1a hashes exactly; RGB expands to
  alpha255, colormap and RGBA inputs decode unchanged. Missing file, a
  non-PNG file and empty/NULL arguments reject with zeroed outputs; stop/start
  republishes and decodes again.
- `check-ui-png-cards`: with no `.rgba` assets, a synthetic DB of the nine real
  names drives `ui_vendor`; the recorder sees `scaled` blits with source
  `400x225` and the first decoded source pixel for every card.
  `artifacts/analysis/image-decode-2026-11-04/pngcards.png` is the resulting
  1920x1080 recorder raster (flattened for viewing).
- Full host `make check` passes (344 ok lines); focused ASan/UBSan
  `check-image-decode` is clean; the decoder test and the UI PNG-card test both
  pass under ARM/qemu using the vendor libpng/zlib files.

## Limits

- Not run on the TV or through the supervised fb1 binding in this step; the
  visible apps-grid gate still needs a live/scanout run with these assets.
- JPEG (`foxxum_thumb_*` and the launcher thumbnails that are JPEG) is not
  decoded; the module name is PNG-specific for now.
- The vendor `hal::PngImageDecoderImpl` transform/limits are not independently
  compared. The output matches libpng 1.6 and the production straight-RGBA
  canvas, not a vendor class differential or stock pixel parity.
- No dynamic DB refresh, scaling policy, APNG or image-decoder error UI is
  added; `ui_engine` still uses nearest-neighbour cell scaling.
