#!/usr/bin/env python3
"""Stage OAI1 blobs for an offline stock-layout apps-grid render.

With --db, use the live LauncherItem rows (parent0, type2, non-deleted,
non-empty thumbnail) and the original downloaded thumbnail names. The special
"Markt" tile is inserted by the capture test as a project row named `markt`.
Without --db, stage the fixed 25-item comparison corpus.
"""
import argparse
import sqlite3
import struct
from pathlib import Path

from PIL import Image, ImageChops

THUMBS = [
    "launcher_app_thumb61", "launcher_app_thumb336",
    "launcher_app_thumb368", "launcher_app_thumb495",
    "launcher_app_thumb356", "launcher_app_thumb355",
    "launcher_app_thumb354", "launcher_app_thumb385",
    "launcher_app_thumb476",
]


def write_oai1(dst: Path, im: Image.Image) -> None:
    im = im.convert("RGBA")
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


class BitmapPack:
    """Parse EF41 records from telefunken18_font_byte_data.bin."""

    def __init__(self, root: Path):
        self.data = (root / "telefunken18_font_byte_data.bin").read_bytes()
        counts = (root / "telefunken18_font_byte_data.txt").read_text().split()
        self.records = []
        off = 0
        for length in (int(x) for x in counts[1:]):
            self.records.append((off, int(length)))
            off += int(length)

    def glyph(self, record: int, char: str):
        off, length = self.records[record]
        rec = self.data[off:off + length]
        pixel_len = int.from_bytes(rec[4:8], "big")
        pixels = 8
        pos = 8 + pixel_len
        if rec[pos:pos + 3] != b"UTF":
            raise ValueError("bad UTF marker")
        key_count = int.from_bytes(rec[pos + 3:pos + 5], "big")
        pos += 5 + key_count * 4
        if rec[pos:pos + 3] != b"DIM":
            raise ValueError("bad DIM marker")
        glyph_count = int.from_bytes(rec[pos + 3:pos + 5], "big")
        pos += 5
        code = ord(char)
        if not 32 <= code <= 126 or code - 32 >= glyph_count:
            raise ValueError("unsupported glyph")
        metrics = pos + (code - 32) * 9
        top, left, bottom, right, advance = struct.unpack(
            "bbbbb", rec[metrics:metrics + 5])
        data_off = int.from_bytes(rec[metrics + 5:metrics + 9], "big")
        w, h = right - left, bottom - top
        if w <= 0 or h <= 0:
            return top, left, bottom, right, advance, None
        coverage = rec[pixels + data_off:pixels + data_off + w * h]
        if len(coverage) != w * h:
            raise ValueError("short glyph")
        return top, left, bottom, right, advance, coverage

    def render(self, record: int, text: str,
               color=(255, 255, 255, 255)) -> Image.Image:
        glyphs = [self.glyph(record, c) for c in text]
        xs, ys = [], []
        pen = 0
        for top, left, bottom, right, advance, _ in glyphs:
            xs += [pen + left, pen + right]
            ys += [top, bottom]
            pen += advance
        minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
        width, height = maxx - minx, maxy - miny
        alpha = Image.new("L", (width, height), 0)
        pen = 0
        for top, left, bottom, right, advance, coverage in glyphs:
            w, h = right - left, bottom - top
            if coverage is not None:
                glyph = Image.frombytes("L", (w, h), coverage)
                x, y = pen + left - minx, top - miny
                crop = alpha.crop((x, y, x + w, y + h))
                alpha.paste(ImageChops.lighter(crop, glyph), (x, y))
            pen += advance
        im = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        im.putdata([(color[0], color[1], color[2], a) for a in alpha.getdata()])
        return im


def turbojpeg_fast_rgba(path: Path) -> Image.Image:
    """Decode a JPEG with TurboJPEG TJFLAG_FASTDCT, the vendor mode.

    VENDOR-DECODER-DIFF-2026-11-04: the shipped hal::JpegImageDecoder matches
    TurboJPEG TJFLAG_FASTDCT byte-exactly, while libjpeg's accurate DCT does
    not. Pillow uses the accurate path, so JPEG thumbnails must not go through
    Image.open for the stock comparison.
    """
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
        raise RuntimeError(f"{path}: TurboJPEG init failed")
    width = ctypes.c_int(0)
    height = ctypes.c_int(0)
    subsamp = ctypes.c_int(0)
    colorspace = ctypes.c_int(0)
    if lib.tjDecompressHeader3(handle, jpeg, ctypes.c_ulong(len(data)),
            ctypes.byref(width), ctypes.byref(height),
            ctypes.byref(subsamp), ctypes.byref(colorspace)):
        lib.tjDestroy(handle)
        raise RuntimeError(f"{path}: TurboJPEG header failed")
    out = (ctypes.c_ubyte * (width.value * height.value * 4))()
    # TJPF_RGBA=7, TJFLAG_FASTDCT=2048.
    if lib.tjDecompress2(handle, jpeg, ctypes.c_ulong(len(data)), out,
            width.value, width.value * 4, height.value, 7, 2048):
        lib.tjDestroy(handle)
        raise RuntimeError(f"{path}: TurboJPEG fast decode failed")
    lib.tjDestroy(handle)
    return Image.frombytes("RGBA", (width.value, height.value), bytes(out))


def load_thumbnail(path: Path) -> Image.Image:
    if path.read_bytes()[:2] == b"\xff\xd8":
        return turbojpeg_fast_rgba(path)
    return Image.open(path)


def live_rows(db: Path):
    con = sqlite3.connect(db)
    rows = con.execute(
        "SELECT m_application_thumb_file_name FROM LauncherItem"
        " WHERE m_parent_id=1 AND m_is_deleted=0 AND m_is_out_of_sync=0"
        " AND m_item_type=2 ORDER BY m_order_index,m_id").fetchall()
    con.close()
    names = []
    for (blob,) in rows:
        if not blob:
            continue
        name = bytes(blob).split(b"\0", 1)[0].decode("utf-8", "replace")
        if name:
            names.append(name)
    return names


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--db")
    ap.add_argument("--thumbs", default=str(root / "artifacts/workbench/thumbs"))
    ap.add_argument("--ui-assets", default=str(root / "artifacts/ui-assets"))
    ap.add_argument("--font", default=str(root / "artifacts/workbench/rootfs/vendor/fonts/DefaultProportional.ttf"))
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    ui = Path(args.ui_assets)
    thumbs = Path(args.thumbs)

    write_oai1(out / "background_fullscreen.rgba",
               Image.open(ui / "background_fullscreen.png"))
    pack = BitmapPack(root / "artifacts/workbench/rootfs/vendor2/dynamic_ui")
    # Verified against the stock capture: title is bitmap font 18 at 81,68;
    # Markt label is bitmap font 4 in (38,23,25) at 308,369.
    write_oai1(out / "apps_title.rgba", pack.render(18, "Apps"))
    write_oai1(out / "markt.rgba", Image.open(ui / "market_animated.png"))
    write_oai1(out / "markt_label.rgba",
               pack.render(4, "Markt", (38, 23, 25, 255)))

    if args.db:
        names = live_rows(Path(args.db))
        for name in names:
            src = thumbs / name
            if not src.exists():
                print(f"missing live thumbnail {src}", flush=True)
                continue
            write_oai1(out / (name + ".rgba"), load_thumbnail(src))
        print(f"staged {len(names)} live thumbnails -> {out}")
    else:
        for i, name in enumerate(THUMBS, start=1):
            src = thumbs / name
            if not src.exists():
                print(f"missing thumbnail {src}", flush=True)
                return 1
            write_oai1(out / f"live_{i:02d}.rgba", load_thumbnail(src))
        for i in range(10, 20):
            write_oai1(out / f"live_{i:02d}.rgba",
                       Image.new("RGBA", (310, 174), (32, 32, 32, 255)))
        print(f"staged fixed corpus -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
