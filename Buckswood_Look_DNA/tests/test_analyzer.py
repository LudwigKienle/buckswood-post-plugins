#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="buckswood-look-dna-") as directory_name:
        directory = Path(directory_name)
        reference = directory / "reference.png"
        profile = directory / "reference.bwlook"
        report = directory / "reference.json"
        image = Image.new("RGB", (96, 54))
        pixels = image.load()
        for y in range(image.height):
            for x in range(image.width):
                pixels[x, y] = (
                    min(255, 24 + x * 2),
                    min(255, 18 + y * 3),
                    min(255, 150 - x),
                )
        image.save(reference)
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts" / "analyze_reference.py"),
                str(reference),
                str(profile),
                "--reference-space",
                "srgb",
                "--quality",
                "draft",
                "--report",
                str(report),
            ],
            check=True,
        )
        data = profile.read_bytes()
        assert data[:8] == b"BWLOOK1\0"
        version, float_count, sample_count = struct.unpack("<III", data[8:20])
        assert version == 1
        assert float_count == 35
        assert sample_count == 96 * 54
        values = struct.unpack("<35f", data[20:160])
        assert values[0] <= values[3] <= values[6]
        assert values[14] > 0.0
        analysis = json.loads(report.read_text(encoding="utf-8"))
        assert analysis["format"] == "BWLOOK1"
        assert analysis["semantic_backend"] == "native-zone-v1"
        profile_reader = os.environ.get("PROFILE_READER")
        if profile_reader:
            subprocess.run([profile_reader, str(profile)], check=True)
    print("Buckswood Look DNA companion analyzer tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
