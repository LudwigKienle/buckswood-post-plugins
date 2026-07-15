#!/usr/bin/env python3
"""Companion temporal reconstruction adapter for Buckswood Film Emulation.

The built-in fallback is intentionally simple and deterministic. For true neural
restoration, call BasicVSR++ or another backend through this wrapper.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image


IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp"}


def smoothstep(edge0: float, edge1: float, value: np.ndarray) -> np.ndarray:
    denominator = max(edge1 - edge0, 1e-6)
    t = np.clip((value - edge0) / denominator, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def sorted_frames(input_dir: Path) -> list[Path]:
    frames = [
        path
        for path in input_dir.iterdir()
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
    ]
    return sorted(frames, key=lambda path: path.name)


def load_image(path: Path) -> np.ndarray:
    image = Image.open(path).convert("RGBA")
    return np.asarray(image, dtype=np.float32) / 255.0


def save_image(path: Path, pixels: np.ndarray) -> None:
    pixels = np.clip(pixels * 255.0 + 0.5, 0.0, 255.0).astype(np.uint8)
    image = Image.fromarray(pixels, mode="RGBA")
    if path.suffix.lower() in {".jpg", ".jpeg"}:
        image = image.convert("RGB")
    image.save(path)


def luma(pixels: np.ndarray) -> np.ndarray:
    return pixels[..., 0] * 0.2627 + pixels[..., 1] * 0.6780 + pixels[..., 2] * 0.0593


def fallback_reconstruct(
    input_dir: Path,
    output_dir: Path,
    strength: float,
    stability: float,
    motion_guard: float,
) -> int:
    frames = sorted_frames(input_dir)
    if not frames:
        raise RuntimeError(f"No image frames found in {input_dir}")

    output_dir.mkdir(parents=True, exist_ok=True)
    cache: dict[int, np.ndarray] = {}

    def frame_at(index: int) -> np.ndarray:
        if index not in cache:
            cache[index] = load_image(frames[index])
        return cache[index]

    for index, source_path in enumerate(frames):
        center = frame_at(index)
        neighbors: list[tuple[np.ndarray, float]] = []
        for offset, weight in ((-2, 0.45), (-1, 1.0), (1, 1.0), (2, 0.45)):
            neighbor_index = index + offset
            if 0 <= neighbor_index < len(frames):
                neighbor = frame_at(neighbor_index)
                if neighbor.shape == center.shape:
                    neighbors.append((neighbor, weight))

        if not neighbors:
            shutil.copy2(source_path, output_dir / source_path.name)
            continue

        weight_sum = sum(weight for _, weight in neighbors)
        average = sum(neighbor * weight for neighbor, weight in neighbors) / weight_sum
        motion = np.abs(luma(center) - luma(average))
        guard = smoothstep(0.018, 0.16, motion)[..., None] * np.clip(motion_guard, 0.0, 1.0)
        blend = np.clip(strength, 0.0, 1.0) * np.clip(stability, 0.0, 1.0) * (1.0 - guard)
        output = center * (1.0 - blend) + average * blend
        output[..., 3] = center[..., 3]
        save_image(output_dir / source_path.name, output)

        if len(cache) > 7:
            for old_index in list(cache):
                if old_index < index - 3:
                    del cache[old_index]

    return len(frames)


def run_basicvsrpp(
    input_dir: Path,
    output_dir: Path,
    basicvsrpp_dir: Path | None,
    config: Path | None,
    checkpoint: Path | None,
) -> None:
    if not basicvsrpp_dir or not config or not checkpoint:
        raise RuntimeError("--basicvsrpp-dir, --config and --checkpoint are required for --mode basicvsrpp")
    if not basicvsrpp_dir.exists():
        raise RuntimeError(f"BasicVSR++ directory does not exist: {basicvsrpp_dir}")
    demo = basicvsrpp_dir / "demo" / "restoration_video_demo.py"
    if not demo.exists():
        raise RuntimeError(f"Cannot find BasicVSR++ demo script: {demo}")

    output_dir.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        str(demo),
        str(config),
        str(checkpoint),
        str(input_dir),
        str(output_dir),
    ]
    subprocess.run(command, cwd=basicvsrpp_dir, check=True)


def run_external(input_dir: Path, output_dir: Path, command_template: str | None) -> None:
    if not command_template:
        raise RuntimeError("--external-command is required for --mode external")
    output_dir.mkdir(parents=True, exist_ok=True)
    command = command_template.format(
        input_dir=str(input_dir),
        output_dir=str(output_dir),
    )
    subprocess.run(command, shell=True, check=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Temporal reconstruction companion for Buckswood Film Emulation.")
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--mode", choices=("fallback", "basicvsrpp", "external"), default="fallback")
    parser.add_argument("--strength", type=float, default=0.30)
    parser.add_argument("--stability", type=float, default=0.70)
    parser.add_argument("--motion-guard", type=float, default=0.85)
    parser.add_argument("--basicvsrpp-dir", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--checkpoint", type=Path)
    parser.add_argument("--external-command")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_dir = args.input_dir.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()

    if not input_dir.exists():
        raise RuntimeError(f"Input path does not exist: {input_dir}")

    if args.mode == "fallback":
        count = fallback_reconstruct(
            input_dir,
            output_dir,
            args.strength,
            args.stability,
            args.motion_guard,
        )
        print(f"Processed {count} frames with fallback temporal reconstruction.")
    elif args.mode == "basicvsrpp":
        run_basicvsrpp(
            input_dir,
            output_dir,
            args.basicvsrpp_dir.expanduser().resolve() if args.basicvsrpp_dir else None,
            args.config.expanduser().resolve() if args.config else None,
            args.checkpoint.expanduser().resolve() if args.checkpoint else None,
        )
    elif args.mode == "external":
        run_external(input_dir, output_dir, args.external_command)
    else:
        raise RuntimeError(f"Unsupported mode: {args.mode}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
