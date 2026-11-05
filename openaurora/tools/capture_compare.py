#!/usr/bin/env python3
"""Compare a stock TV capture with a candidate render.

The TV capture path saves ordinary RGB PNGs after decoding the 32bpp OSD
layer as BGRA. Old pre-correction captures store red and blue swapped: pass
``--reference-order BGR`` for those files instead of editing them first.

Metrics intentionally match the earlier STOCK-PARITY measurements:
  AE      = mean absolute error / 255 * pixels
  RMSE16  = root-mean-square error * 257
MAE8/RMSE8 and per-region values are also reported.
"""
import argparse
import json
import sys

from PIL import Image, ImageChops, ImageStat


def load(path, order, label):
    try:
        image = Image.open(path).convert("RGB")
    except OSError as error:
        raise SystemExit(f"{label}: cannot read {path}: {error}")
    if order == "BGR":
        red, green, blue = image.split()
        image = Image.merge("RGB", (blue, green, red))
    return image


def size(value):
    try:
        width, height = (int(part) for part in value.lower().split("x", 1))
    except ValueError:
        raise argparse.ArgumentTypeError("expected WIDTHxHEIGHT")
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("width and height must be positive")
    return width, height


def metrics(reference, candidate, tolerance):
    diff = ImageChops.difference(reference, candidate)
    stat = ImageStat.Stat(diff)
    pixels = reference.width * reference.height
    samples = pixels * 3
    sum_abs = sum(stat.sum)
    mae8 = sum_abs / samples
    rmse8 = (sum(value * value for value in stat.rms) / 3.0) ** 0.5
    maximum = max(high for _, high in stat.extrema)
    changed = 0
    if tolerance >= 0:
        data = diff.tobytes()
        for offset in range(0, len(data), 3):
            if (data[offset] > tolerance or data[offset + 1] > tolerance or
                    data[offset + 2] > tolerance):
                changed += 1
    return {
        "pixels": pixels,
        "mae8": mae8,
        "rmse8": rmse8,
        "mae_norm": mae8 / 255.0,
        "rmse_norm": rmse8 / 255.0,
        "ae": mae8 / 255.0 * pixels,
        "rmse16": rmse8 * 257.0,
        "max_delta": maximum,
        "changed_pixels": changed,
        "changed_ratio": changed / pixels if pixels else 0.0,
    }


def show(name, values):
    print(f"{name}: AE={values['ae']:.6g} ({values['mae_norm']:.6f}) "
          f"RMSE16={values['rmse16']:.6g} MAE8={values['mae8']:.6g} "
          f"RMSE8={values['rmse8']:.6g} max={values['max_delta']} "
          f"changed={values['changed_pixels']}/{values['pixels']} "
          f"({values['changed_ratio']:.6f})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, help="stock capture PNG")
    parser.add_argument("--candidate", required=True, help="candidate render PNG")
    parser.add_argument("--reference-order", choices=["RGB", "BGR"], default="RGB",
                        help="stored channel order; BGR corrects an old red/blue-swapped capture")
    parser.add_argument("--candidate-order", choices=["RGB", "BGR"], default="RGB")
    parser.add_argument("--manifest", help="JSON with a regions list of {name,x,y,w,h}")
    parser.add_argument("--json", help="write the metrics as JSON to this path")
    parser.add_argument("--size", type=size, default=(1920, 1080),
                        help="required capture size (default 1920x1080)")
    parser.add_argument("--tolerance", type=int, default=0,
                        help="per-channel delta counted as changed (default 0)")
    args = parser.parse_args()

    reference = load(args.reference, args.reference_order, "reference")
    candidate = load(args.candidate, args.candidate_order, "candidate")
    if reference.size != args.size:
        raise SystemExit(f"reference is {reference.width}x{reference.height}, "
                         f"expected {args.size[0]}x{args.size[1]}")
    if candidate.size != args.size:
        raise SystemExit(f"candidate is {candidate.width}x{candidate.height}, "
                         f"expected {args.size[0]}x{args.size[1]}")

    result = {
        "reference": {"path": args.reference, "order": args.reference_order,
                      "width": reference.width, "height": reference.height},
        "candidate": {"path": args.candidate, "order": args.candidate_order,
                      "width": candidate.width, "height": candidate.height},
        "tolerance": args.tolerance,
        "full": metrics(reference, candidate, args.tolerance),
        "regions": [],
    }
    show("full", result["full"])

    if args.manifest:
        try:
            with open(args.manifest, encoding="utf-8") as stream:
                manifest = json.load(stream)
        except (OSError, ValueError) as error:
            raise SystemExit(f"manifest: {error}")
        regions = manifest.get("regions")
        if not isinstance(regions, list) or not regions:
            raise SystemExit("manifest: regions must be a non-empty list")
        seen = set()
        for item in regions:
            try:
                name = item["name"]
                x, y, w, h = (int(item[key]) for key in ("x", "y", "w", "h"))
            except (KeyError, TypeError, ValueError):
                raise SystemExit(f"manifest: bad region {item!r}")
            if not name or name in seen:
                raise SystemExit(f"manifest: duplicate or empty region name {name!r}")
            seen.add(name)
            if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > reference.width or \
                    y + h > reference.height:
                raise SystemExit(f"manifest: region {name!r} is outside the image")
            box = (x, y, x + w, y + h)
            values = metrics(reference.crop(box), candidate.crop(box), args.tolerance)
            result["regions"].append({"name": name, "x": x, "y": y,
                                      "w": w, "h": h, "metrics": values})
            show(f"region {name}", values)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")


if __name__ == "__main__":
    main()
