#!/usr/bin/env python3
"""Offline vendor PngImageDecoderImpl/JpegImageDecoder differential.

Runs the shipped ARM decoder classes inside qemu via an LD_PRELOAD probe.
No TV or HAL device is opened. PNG output must be byte-exact after BGRA->RGBA
(or palette-index) normalization; JPEG is checked with a small codec-rounding
tolerance because the vendor class uses libjpeg's scanline conversion rather
than TurboJPEG's exact path.
"""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

try:
    from PIL import Image
except ImportError:
    print("skipping: Pillow unavailable", file=sys.stderr)
    sys.exit(77)

MAGIC = 0x56444543
PNG_OK = [
    "launcher_app_thumb336", "launcher_app_thumb354",
    "launcher_app_thumb355", "launcher_app_thumb356",
    "launcher_app_thumb368", "launcher_app_thumb385",
    "launcher_app_thumb476", "launcher_app_thumb495",
    "launcher_app_thumb61",
]
JPEG_OK = ["DMR-small.jpg", "DMR-large.jpg"]


def fail(message):
    print(message, file=sys.stderr)
    raise SystemExit(1)


def vendor_libdirs(root):
    vendor = root / "artifacts/workbench/rootfs/vendor"
    dirs = set()
    for path in vendor.rglob("*.so*"):
        if path.is_file() or path.is_symlink():
            dirs.add(str(path.parent))
    return sorted(dirs)


def run_probe(qemu, sysroot, aurora, probe, libdirs, fmt, path):
    with tempfile.NamedTemporaryFile(prefix="oa-vdec-", delete=False) as f:
        out = f.name
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join(libdirs)
    env["LD_PRELOAD"] = str(probe)
    env["OA_VENDOR_FORMAT"] = fmt
    env["OA_VENDOR_INPUT"] = str(path)
    env["OA_VENDOR_OUTPUT"] = out
    try:
        subprocess.run(
            [qemu, "-L", str(sysroot), str(aurora)],
            env=env, check=True, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, timeout=60,
        )
        data = Path(out).read_bytes()
    finally:
        Path(out).unlink(missing_ok=True)
    if len(data) < 24:
        fail(f"{path.name}: short vendor result")
    magic, rc, width, height, rowlen, _ = struct.unpack("<6I", data[:24])
    if magic != MAGIC:
        fail(f"{path.name}: bad vendor result magic")
    if rc != 0 or width <= 0 or height <= 0 or rowlen <= 0:
        fail(f"{path.name}: vendor decode failed rc={rc} {width}x{height} row={rowlen}")
    pixels = data[24:]
    if len(pixels) < rowlen * height:
        fail(f"{path.name}: vendor pixels truncated")
    return width, height, rowlen, pixels


def bgra_to_rgba(pixels, width, height, rowlen):
    if rowlen != width * 4:
        fail(f"unexpected rgba row length {rowlen} for {width}")
    out = bytearray(width * height * 4)
    for y in range(height):
        src = y * rowlen
        dst = y * width * 4
        for x in range(width):
            b, g, r, a = pixels[src + x*4:src + x*4 + 4]
            out[dst + x*4:dst + x*4 + 4] = bytes((r, g, b, a))
    return bytes(out)


def palette_indices(pixels, width, height, rowlen):
    if rowlen != width:
        fail(f"unexpected palette row length {rowlen} for {width}")
    return bytes(pixels[:width * height])


def check_png(name, root, probe, common):
    path = root / "artifacts/workbench/thumbs" / name
    width, height, rowlen, pixels = run_probe(*common, "png", path)
    im = Image.open(path)
    if im.size != (width, height):
        fail(f"{name}: vendor size {width}x{height} != {im.size}")
    if im.mode == "P":
        ref = im.tobytes()
        got = palette_indices(pixels, width, height, rowlen)
        if got != ref:
            fail(f"{name}: palette indices differ")
        return "palette-index exact"
    ref = im.convert("RGBA").tobytes()
    got = bgra_to_rgba(pixels, width, height, rowlen)
    if got != ref:
        diff = sum(a != b for a, b in zip(got, ref))
        fail(f"{name}: PNG BGRA differs from RGBA reference ({diff} bytes)")
    return "BGRA->RGBA exact"


def check_jpeg(name, root, probe, common, magick, tolerance):
    path = root / "artifacts/dumps/apdat_files/dlna-dmr/xml/icon" / name
    width, height, rowlen, pixels = run_probe(*common, "jpeg", path)
    ref = subprocess.check_output([magick, str(path), "-depth", "8", "RGBA:-"])
    if len(ref) != width * height * 4:
        fail(f"{name}: reference size {len(ref)} != {width*height*4}")
    got = bgra_to_rgba(pixels, width, height, rowlen)
    max_delta = 0
    total = 0
    for i in range(0, len(got), 4):
        if got[i+3] != ref[i+3]:
            fail(f"{name}: alpha differs at pixel {i//4}")
        for c in range(3):
            d = abs(got[i+c] - ref[i+c])
            max_delta = max(max_delta, d)
            total += d
    mean = total / (width * height * 3)
    if max_delta > tolerance:
        fail(f"{name}: JPEG max delta {max_delta} > {tolerance}")
    return f"max channel delta {max_delta}, mean {mean:.3f}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default=os.environ.get("BUILD", "build"))
    parser.add_argument("--qemu", default="qemu-arm-static")
    parser.add_argument("--tolerance", type=int, default=8)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = Path(args.build)
    if not build.is_absolute():
        build = root / "openaurora" / build
    probe = build / "tests/vendor_decoder_probe.so"
    if not probe.exists():
        fail(f"missing probe {probe}; build it first")
    qemu = shutil.which(args.qemu)
    if not qemu:
        fail(f"missing {args.qemu}")
    sysroot = root / ("artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-"
        "arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc")
    aurora = root / "artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf"
    magick = shutil.which("magick")
    if not aurora.exists():
        fail(f"missing {aurora}")
    if not sysroot.exists():
        fail(f"missing {sysroot}")
    if not magick:
        fail("missing ImageMagick")
    common = (qemu, sysroot, aurora, probe, vendor_libdirs(root))
    for name in PNG_OK:
        print(f"ok vendor png {name}: {check_png(name, root, probe, common)}")
    for name in JPEG_OK:
        print(f"ok vendor jpeg {name}: {check_jpeg(name, root, probe, common, magick, args.tolerance)}")
    print(f"ok vendor decoder differential: {len(PNG_OK)} PNG, {len(JPEG_OK)} JPEG")


if __name__ == "__main__":
    main()
