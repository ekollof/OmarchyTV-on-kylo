# Downloaded PNG and JPEG decoding — 2026-11-04

**Follow-up after TODO-REAUDIT-2026-11-04:** the reopened PNG item was
narrowed by moving the decoder to libpng's classic API with Aurora's recovered
`PngImageDecoderImpl::setConversions` sequence (b30918), and the service is now
`image.decode` v2 with a vendor-libjpeg-turbo JPEG path. The earlier
simplified-API prototype results are historical; the tests below exercise the
classic build. Vendor-class differential, APNG/CMYK policy, on-TV decode and
stock pixel parity remain open.

`image_decode` is a reloadable module publishing `image.decode` v2. It decodes
the vendor-downloaded PNG and JPEG assets in `/buffer/downloaded_images` into
straight RGBA, and `ui_engine` falls back to it when `<ui.dir>/<name>.rgba` is
absent. No TV access was made after the owner's stop instruction; all evidence
is offline.

## Firmware basis

`aurora.elf` contains `hal::PngImageDecoderImpl` (`0xb2f6a0` region) with
`setConversions` at `0xb30918`: packing/low-depth expansion, strip16 (not
scale16), gray/palette to RGB, tRNS to alpha or opaque filler, and no gamma
conversion request. It also contains `hal::JpegImageDecoder` and a statically
linked `cabot_jpeg`/`cabot_png`. The rootfs carries
`/vendor/lib/libs/libpng16.so.16` (1.6.28), `libz.so`, and
`/vendor/com.nvt.nflx/lib/libturbojpeg.so.0` (libjpeg-turbo 2.0.1); Aurora
itself also links `libjpeg.so.9`.

The module uses the shared libpng classic API and the vendor TurboJPEG API
(`tjInitDecompress`, `tjDecompressHeader3`, `tjDecompress2`, `tjDestroy`).
This matches the shipped conversion sequence and codec family, but is still not
a differential against the vendor decoder classes.

## Implementation

- `modules/image_decode/image_decode.c`: `image.decode` v2 with
  `decode_png` and `decode_jpeg`, straight top-down RGBA, 16 MiB input,
  8192-pixel axis and 64 MiB output bounds. Regular files only. PNG animated
  chunks (`acTL`) are rejected; JPEG CMYK/YCCK is rejected instead of being
  converted through an invented color-managed path. Input is bounded/read into
  memory; libpng errors longjmp and all buffers/libraries are released.
  Suspend returns `-EAGAIN`; stop/start and `reload` republish the library.
- `ui_engine`: OAI1 `<ui.dir>/<name>.rgba` remains first; on a miss it looks
  up version 2, tries PNG then JPEG, copies the decoded pixels into the engine
  cache, then calls the provider release while the hold is still active.
- `tools/boot/openaurora.conf` autoloads `image_decode` before `ui_engine`.

## Verification

- `check-image-decode`: nine real downloaded PNGs (RGB, RGBA, colormap,
  400x225) match independent ImageMagick/libpng RGBA SHA256 and FNV-1a exactly.
  The two DMR JPEGs use `TJFLAG_FASTDCT`; this matches the shipped
  `JpegImageDecoder` class exactly (max channel delta 0), while ImageMagick's
  accurate-DCT output differs by up to 8. Missing paths, a real non-PNG ELF
  file, truncated PNG/JPEG prefixes and empty/NULL arguments reject with zeroed
  outputs. A `reload` succeeds after release; suspend returns `-EAGAIN` and
  resume restores decoding.
- `check-ui-png-cards` / `check-ui-jpeg-cards`: with no `.rgba` assets, a
  synthetic DB drives `ui_vendor`; the recorder sees scaled blits with the
  decoded source dimensions and first source pixels.
  `artifacts/analysis/image-decode-2026-11-04/pngcards.png` and
  `jpegcards.png` are the flattened 1920x1080 recorder rasters.
- Full host `make check` passes (395 ok lines); focused ASan/UBSan for the
  decoder and JPEG-card path is clean; decoder and UI tests pass under
  ARM/qemu with the vendor libpng/zlib and libjpeg-turbo 2.0.1.
- `check-vendor-decoder`: the shipped `PngImageDecoderImpl` and
  `JpegImageDecoder` classes were executed under qemu; all nine PNGs plus
  grayscale/palette/tRNS/16-bit/interlaced/gAMA variants are exact, and the
  DMR JPEGs are exact under fast DCT. See VENDOR-DECODER-DIFF.

## Limits

- The actual `/buffer/downloaded_images/foxxum_thumb_*` JPEGs were not copied
  offline before TV access stopped. The JPEG corpus uses other real vendor JPEG
  files from the dump/rootfs; actual downloaded JPEGs still need an offline
  copy or an allowed on-TV read.
- Vendor classes are controlled-call probes (no vendor UI/global constructors),
  not full image-player worker/frame-lifetime execution. On-TV decode and stock
  pixel parity remain open.
- APNG is rejected; JPEG CMYK/YCCK is rejected. `ui_engine` still uses
  nearest-neighbour cell scaling.
