#!/usr/bin/env python3
"""Analyze the BGRA frames written by tools/display/fb-region-sampler.c.

usage: fb-pulse-compare.py OUTDIR WIDTH HEIGHT [--threshold N] [--json PATH]

The first frame is the baseline. Every later frame is compared by channel
difference; the changed bounding box, pixel count and mean absolute difference
are printed with the sampler's relative timestamp. This is a read-only analysis
of captured bytes; it does not open devices.
"""
import argparse
import json
import os
import sys

try:
    from PIL import Image, ImageChops, ImageStat
except ImportError:
    sys.exit("Pillow is required")


def load_manifest(path, width, height):
    frames = []
    frame_bytes = width * height * 4
    with open(path, "rb") as data:
        for line in open(os.path.join(os.path.dirname(path), "manifest.txt")):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) != 4:
                raise SystemExit(f"bad manifest line: {line!r}")
            index, t_ms, yoff, digest = fields
            raw = data.read(frame_bytes)
            if len(raw) != frame_bytes:
                raise SystemExit(f"short frame {index}: {len(raw)} bytes")
            frames.append((int(index), int(t_ms), int(yoff), digest, raw))
    return frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("width", type=int)
    ap.add_argument("height", type=int)
    ap.add_argument("--threshold", type=int, default=8)
    ap.add_argument("--json", default="")
    args = ap.parse_args()
    path = os.path.join(args.outdir, "frames.bgra")
    frames = load_manifest(path, args.width, args.height)
    if not frames:
        raise SystemExit("no frames")
    base = Image.frombytes("RGBA", (args.width, args.height), frames[0][4])
    rows = []
    last_change = None
    for index, t_ms, yoff, digest, raw in frames:
        image = Image.frombytes("RGBA", (args.width, args.height), raw)
        diff = ImageChops.difference(base, image).convert("L")
        mask = diff.point(lambda v: 255 if v > args.threshold else 0)
        bbox = mask.getbbox()
        mean = ImageStat.Stat(diff).mean[0]
        changed = 0 if bbox is None else mask.histogram()[255]
        if bbox:
            last_change = t_ms
        row = {
            "index": index,
            "t_ms": t_ms,
            "yoffset": yoff,
            "bbox": bbox,
            "changed": changed,
            "mean_abs": round(mean, 4),
            "hash": digest,
        }
        rows.append(row)
        box = "-" if bbox is None else "%d,%d %dx%d" % (
            bbox[0], bbox[1], bbox[2] - bbox[0], bbox[3] - bbox[1])
        print("frame %3d t=%5dms yoff=%d changed=%6d bbox=%-18s mean=%7.3f" % (
            index, t_ms, yoff, changed, box, mean))
    changed_rows = [r for r in rows if r["bbox"]]
    if changed_rows:
        xs = [r["bbox"][0] for r in changed_rows]
        ys = [r["bbox"][1] for r in changed_rows]
        xe = [r["bbox"][2] for r in changed_rows]
        ye = [r["bbox"][3] for r in changed_rows]
        print("changed union: %d,%d %dx%d; last change t=%dms" % (
            min(xs), min(ys), max(xe) - min(xs), max(ye) - min(ys), last_change))
    else:
        print("no changes above threshold")
    if args.json:
        with open(args.json, "w") as out:
            json.dump(rows, out, indent=2, sort_keys=True)
            out.write("\n")


if __name__ == "__main__":
    main()
