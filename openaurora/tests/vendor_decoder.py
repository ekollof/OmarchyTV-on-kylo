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
    env["OA_VENDOR_MODEL"] = "3"  # requested RGBA ImageProperties
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


def check_png_path(name, path, common):
    width, height, rowlen, pixels = run_probe(*common, "png", path)
    im = Image.open(path)
    if im.size != (width, height):
        fail(f"{name}: vendor size {width}x{height} != {im.size}")
    if rowlen != width * 4:
        fail(f"{name}: requested-RGBA row length {rowlen} != {width*4}")
    ref = im.convert("RGBA").tobytes()
    got = pixels[:width * height * 4]
    if got != ref:
        diff = sum(a != b for a, b in zip(got, ref))
        fail(f"{name}: PNG RGBA differs from reference ({diff} bytes)")
    return "RGBA exact"


def check_png(name, root, common):
    return check_png_path(name, root / "artifacts/workbench/thumbs" / name,
        common)


def make_variants(source, directory, magick):
    variants = [
        ("gray1", ["-colorspace", "Gray", "-depth", "1",
                   "-define", "png:bit-depth=1"]),
        ("gray2", ["-colorspace", "Gray", "-depth", "2",
                   "-define", "png:bit-depth=2"]),
        ("gray4", ["-colorspace", "Gray", "-depth", "4",
                   "-define", "png:bit-depth=4"]),
        ("gray8", ["-colorspace", "Gray", "-depth", "8",
                   "-define", "png:color-type=0"]),
        ("graya8", ["-colorspace", "Gray", "-alpha", "set", "-depth", "8",
                    "-define", "png:color-type=4"]),
        ("palette", ["-colors", "16"]),
        ("palette_trns", ["-colors", "16", "-transparent", "white"]),
        ("rgb16", ["-depth", "16", "-define", "png:bit-depth=16",
                   "-define", "png:color-type=2"]),
        ("rgba16", ["-depth", "16", "-define", "png:bit-depth=16",
                    "-define", "png:color-type=6"]),
        ("interlaced", ["-colors", "16", "-interlace", "PNG"]),
        ("gamma", ["-set", "gamma", "0.45455"]),
    ]
    out = []
    for name, args in variants:
        path = directory / f"{name}.png"
        subprocess.run([magick, str(source), *args, str(path)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out.append((name, path))
    return out


def turbojpeg_fast_rgba(path):
    import ctypes
    lib = ctypes.CDLL("libturbojpeg.so.0")
    lib.tjInitDecompress.restype = ctypes.c_void_p
    lib.tjDecompressHeader3.argtypes = [ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_ubyte), ctypes.c_ulong,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.tjDecompress2.argtypes = [ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_ubyte), ctypes.c_ulong,
        ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int, ctypes.c_int,
        ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.tjDestroy.argtypes = [ctypes.c_void_p]
    data = path.read_bytes()
    jpeg = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
    handle = lib.tjInitDecompress()
    if not handle:
        fail("TurboJPEG init failed")
    w = ctypes.c_int(0); h = ctypes.c_int(0)
    sub = ctypes.c_int(0); colorspace = ctypes.c_int(0)
    if lib.tjDecompressHeader3(handle, jpeg, ctypes.c_ulong(len(data)),
            ctypes.byref(w), ctypes.byref(h), ctypes.byref(sub),
            ctypes.byref(colorspace)):
        fail(f"{path.name}: TurboJPEG header failed")
    out = (ctypes.c_ubyte * (w.value * h.value * 4))()
    # TJPF_RGBA=7, TJFLAG_FASTDCT=2048.
    if lib.tjDecompress2(handle, jpeg, ctypes.c_ulong(len(data)), out,
            w.value, w.value * 4, h.value, 7, 2048):
        fail(f"{path.name}: TurboJPEG fast decode failed")
    lib.tjDestroy(handle)
    return w.value, h.value, bytes(out)


def check_jpeg(name, root, common, tolerance):
    path = root / "artifacts/dumps/apdat_files/dlna-dmr/xml/icon" / name
    width, height, rowlen, pixels = run_probe(*common, "jpeg", path)
    ref_w, ref_h, ref = turbojpeg_fast_rgba(path)
    if (width, height) != (ref_w, ref_h):
        fail(f"{name}: vendor size != TurboJPEG reference")
    if rowlen != width * 4 or len(ref) != width * height * 4:
        fail(f"{name}: unexpected row/reference size")
    got = pixels[:width * height * 4]
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
    parser.add_argument("--tolerance", type=int, default=0)
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
        print(f"ok vendor png {name}: {check_png(name, root, common)}")
    base = root / "artifacts/workbench/thumbs/launcher_app_thumb61"
    with tempfile.TemporaryDirectory(prefix="oa-png-variants-") as tmp:
        for name, path in make_variants(base, Path(tmp), magick):
            print(f"ok vendor png variant {name}: {check_png_path(name, path, common)}")
    for name in JPEG_OK:
        print(f"ok vendor jpeg {name}: {check_jpeg(name, root, common, args.tolerance)}")
    print(f"ok vendor decoder differential: {len(PNG_OK)} PNG + variants, {len(JPEG_OK)} JPEG")


if __name__ == "__main__":
    main()
