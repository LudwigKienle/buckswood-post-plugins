#!/usr/bin/env python3

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "radiance_cache.py"


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="buckswood-cache-test-") as temporary:
        root = Path(temporary)
        source = root / "source"
        cache = root / "cache"
        source.mkdir()

        for index in range(5):
            image = np.full((24, 32, 3), 0.18, dtype=np.float32)
            image[5:18, 8 + index:20 + index] = (0.98, 0.86, 0.64)
            image[0:4] = 1.0
            Image.fromarray(np.uint8(np.clip(image, 0.0, 1.0) * 255.0)).save(
                source / f"shot_{index:04d}.png"
            )

        subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "build",
                "--source-dir",
                str(source),
                "--output-dir",
                str(cache),
                "--start-frame",
                "100",
            ],
            check=True,
        )
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "validate",
                "--cache-dir",
                str(cache),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        summary = json.loads(result.stdout)
        assert summary["ok"] is True
        assert summary["frame_count"] == 5
        assert summary["dimensions"] == [32, 24]
        assert summary["first_frame"] == 100
        assert summary["last_frame"] == 104

        cache_file = cache / "frame_00000102.bwrcache"
        expected_size = 32 + 32 * 24 * 8
        assert cache_file.stat().st_size == expected_size
        inspect = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "inspect",
                "--cache-file",
                str(cache_file),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        values = json.loads(inspect.stdout)
        assert values["rgb_max"] > 1.0
        assert 0.0 <= values["confidence_mean"] <= 1.0
    print("Buckswood Radiance Cache companion tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
