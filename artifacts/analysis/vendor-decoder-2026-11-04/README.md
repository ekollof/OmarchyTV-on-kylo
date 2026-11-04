# Vendor decoder differential evidence

Interpretation: [VENDOR-DECODER-DIFF](../../reports/VENDOR-DECODER-DIFF-2026-11-04.md).

- `check.txt`: ARM/qemu vendor class probe over nine PNGs and two JPEGs.
- `identities.txt`: SHA256 of the probe and `aurora.elf`.

The probe is an ARM `LD_PRELOAD` constructor that runs before the vendor main.
It constructs the vendor `PngImageDecoderImpl`/`JpegImageDecoder` with vendor
`MemoryBufferSource`/`MutableImage`, decodes one file, and writes the raw
`width,height,rowlen` result. No TV, framebuffer, HAL device or vendor UI is
opened.

Run:

```sh
make -C openaurora TARGET=tv BUILD=build-tv-v3/vendor-decoder \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc \
  check-vendor-decoder
```

PNG is byte-exact after BGRA→RGBA (or palette-index) normalization. JPEG is
within max channel delta8/mean1.3–1.7, so exact JPEG class parity remains open.
