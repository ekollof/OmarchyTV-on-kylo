#!/usr/bin/env python3
"""Offline tests for openaurora/tools/capture_compare.py. No TV/network."""
import json
import math
import pathlib
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "openaurora/tools/capture_compare.py"


def run(*args):
    return subprocess.run([sys.executable, str(TOOL), *map(str, args)],
                          capture_output=True, text=True)


def pattern(width=8, height=6):
    image = Image.new("RGB", (width, height))
    image.putdata([((x * 17 + y * 5) % 200, (x * 7 + y * 23) % 200,
                    (x * 11 + y * 13) % 200) for y in range(height) for x in range(width)])
    return image


def bgr(image):
    red, green, blue = image.split()
    return Image.merge("RGB", (blue, green, red))


def exact_metrics():
    with tempfile.TemporaryDirectory(prefix="capture-compare-") as directory:
        directory = pathlib.Path(directory)
        reference = pattern()
        reference.save(directory / "reference.png")
        identical = directory / "identical.png"
        reference.save(identical)
        output = directory / "identical.json"
        result = run("--reference", directory / "reference.png", "--candidate", identical,
                     "--size", "8x6", "--json", output)
        assert result.returncode == 0, result.stderr
        values = json.loads(output.read_text())["full"]
        assert values["ae"] == 0 and values["rmse16"] == 0
        assert values["max_delta"] == 0 and values["changed_pixels"] == 0

        changed = reference.copy()
        base = reference.getpixel((1, 1))
        changed.putpixel((1, 1), tuple(base[i] + (3, 4, 9)[i] for i in range(3)))
        changed.save(directory / "changed.png")
        output = directory / "changed.json"
        result = run("--reference", directory / "reference.png", "--candidate",
                     directory / "changed.png", "--size", "8x6", "--json", output)
        assert result.returncode == 0, result.stderr
        values = json.loads(output.read_text())["full"]
        assert math.isclose(values["mae8"], 16 / 144, rel_tol=0, abs_tol=1e-12)
        assert math.isclose(values["ae"], 16 / 144 / 255 * 48, rel_tol=0, abs_tol=1e-12)
        assert math.isclose(values["rmse8"], math.sqrt(106 / 144), rel_tol=0, abs_tol=1e-12)
        assert math.isclose(values["rmse16"], math.sqrt(106 / 144) * 257, rel_tol=0, abs_tol=1e-12)
        assert values["max_delta"] == 9
        assert values["changed_pixels"] == 1
        assert math.isclose(values["changed_ratio"], 1 / 48, rel_tol=0, abs_tol=1e-12)

        # The old capture defect swaps red and blue; BGR order must undo it.
        bgr(reference).save(directory / "bgr.png")
        result = run("--reference", directory / "bgr.png", "--candidate",
                     directory / "reference.png", "--reference-order", "BGR",
                     "--size", "8x6")
        assert result.returncode == 0, result.stderr
        assert "full: AE=0" in result.stdout, result.stdout

        manifest = directory / "regions.json"
        manifest.write_text(json.dumps({"regions": [
            {"name": "changed", "x": 1, "y": 1, "w": 2, "h": 2}]}))
        output = directory / "region.json"
        result = run("--reference", directory / "reference.png", "--candidate",
                     directory / "changed.png", "--size", "8x6",
                     "--manifest", manifest, "--json", output)
        assert result.returncode == 0, result.stderr
        region = json.loads(output.read_text())["regions"][0]
        assert region["name"] == "changed"
        assert region["metrics"]["pixels"] == 4
        assert region["metrics"]["changed_pixels"] == 1

        bad = directory / "bad.json"
        bad.write_text(json.dumps({"regions": [
            {"name": "outside", "x": 7, "y": 0, "w": 2, "h": 1}]}))
        result = run("--reference", directory / "reference.png", "--candidate",
                     directory / "reference.png", "--size", "8x6", "--manifest", bad)
        assert result.returncode != 0 and "outside the image" in result.stderr

        small = directory / "small.png"
        Image.new("RGB", (4, 4)).save(small)
        result = run("--reference", directory / "reference.png", "--candidate", small,
                     "--size", "8x6")
        assert result.returncode != 0 and "expected 8x6" in result.stderr

    stock = ROOT / "artifacts/analysis/stock-parity-2026-11-04/stock-corrected.png"
    portal = ROOT / "artifacts/analysis/ui-live-cards-2026-11-04/offline-portal.png"
    manifest = ROOT / "artifacts/analysis/stock-parity-2026-11-05/portal-cards.json"
    with tempfile.TemporaryDirectory(prefix="capture-compare-real-") as directory:
        output = pathlib.Path(directory) / "real.json"
        result = run("--reference", stock, "--candidate", portal,
                     "--manifest", manifest, "--json", output)
        assert result.returncode == 0, result.stderr
        metrics = {item["name"]: item["metrics"]
                   for item in json.loads(output.read_text())["regions"]}
        assert math.isclose(metrics["netflix"]["ae"], 2784.68, rel_tol=0, abs_tol=0.01)
        assert math.isclose(metrics["netflix"]["rmse16"], 24663.9, rel_tol=0, abs_tol=0.1)
        assert math.isclose(metrics["canal"]["ae"], 311.746, rel_tol=0, abs_tol=0.01)

    print("ok stock capture compare: exact/changed metrics, BGR correction, regions, "
          "bounds/size errors and STOCK-PARITY regression")


if __name__ == "__main__":
    exact_metrics()
