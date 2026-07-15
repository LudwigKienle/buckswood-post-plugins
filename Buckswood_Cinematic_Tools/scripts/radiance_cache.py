#!/usr/bin/env python3
"""Build and validate Buckswood Radiance Recover v2 cache sequences."""

from __future__ import annotations

import argparse
import json
import math
import os
import struct
import sys
from pathlib import Path

try:
    import numpy as np
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Missing companion dependencies. Run scripts/setup_ml_backend.sh first."
    ) from exc


MAGIC = b"BWRCV2\0\0"
VERSION = 2
HEADER = struct.Struct("<8sIIIqI")
EXTENSIONS = {".png", ".tif", ".tiff", ".jpg", ".jpeg", ".bmp", ".webp"}


def smoothstep(edge0: float, edge1: float, value: np.ndarray) -> np.ndarray:
    denominator = max(1.0e-8, edge1 - edge0)
    t = np.clip((value - edge0) / denominator, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def collect_images(directory: Path) -> list[Path]:
    if not directory.is_dir():
        raise ValueError(f"Image directory does not exist: {directory}")
    files = sorted(
        path for path in directory.iterdir()
        if path.is_file() and path.suffix.lower() in EXTENSIONS
    )
    if not files:
        raise ValueError(f"No supported images found in {directory}")
    return files


def load_rgb(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        array = np.asarray(image)
        if array.ndim != 3 or array.shape[2] < 3:
            array = np.asarray(image.convert("RGB"))
        else:
            array = array[..., :3]
    if np.issubdtype(array.dtype, np.integer):
        maximum = float(np.iinfo(array.dtype).max)
        return array.astype(np.float32) / maximum
    return array.astype(np.float32)


def cross_blur(image: np.ndarray) -> np.ndarray:
    padded = np.pad(image, ((1, 1), (1, 1), (0, 0)), mode="edge")
    return (
        padded[1:-1, 1:-1]
        + padded[:-2, 1:-1]
        + padded[2:, 1:-1]
        + padded[1:-1, :-2]
        + padded[1:-1, 2:]
    ) * 0.2


def luma(image: np.ndarray) -> np.ndarray:
    return (
        image[..., 0] * 0.2126
        + image[..., 1] * 0.7152
        + image[..., 2] * 0.0722
    )


def pq_to_relative_linear(pq: np.ndarray, reference_white: float) -> np.ndarray:
    m1 = 2610.0 / 16384.0
    m2 = 2523.0 / 32.0
    c1 = 3424.0 / 4096.0
    c2 = 2413.0 / 128.0
    c3 = 2392.0 / 128.0
    signal = np.clip(pq.astype(np.float32), 0.0, 1.0)
    power = np.power(signal, 1.0 / m2)
    numerator = np.maximum(power - c1, 0.0)
    denominator = np.maximum(c2 - c3 * power, 1.0e-8)
    nits = 10000.0 * np.power(numerator / denominator, 1.0 / m1)
    return nits / max(1.0, reference_white)


def native_temporal_recovery(
    frames: list[np.ndarray],
    index: int,
    headroom_stops: float,
    strength: float,
    temporal_strength: float,
) -> tuple[np.ndarray, np.ndarray]:
    current = frames[index]
    current_y = np.maximum(luma(current), 0.0)
    blurred = cross_blur(current)
    blurred_y = np.maximum(luma(blurred), 0.0)

    indices = [
        max(0, min(len(frames) - 1, index + offset))
        for offset in (-2, -1, 0, 1, 2)
    ]
    temporal_luma = np.stack([luma(frames[item]) for item in indices], axis=0)
    temporal_median = np.median(temporal_luma, axis=0)
    temporal_deviation = np.mean(
        np.abs(temporal_luma - temporal_median[None, ...]),
        axis=0,
    )
    agreement = np.exp(-temporal_deviation * 12.0).astype(np.float32)

    maximum = np.max(current, axis=2)
    minimum = np.min(current, axis=2)
    highlight = smoothstep(0.68, 0.99, current_y)
    clipped = smoothstep(0.94, 1.005, maximum)
    shadow = 1.0 - smoothstep(0.025, 0.24, current_y)
    specular = clipped * smoothstep(
        0.02,
        0.24,
        maximum - minimum + np.abs(current_y - blurred_y),
    )

    headroom = max(0.0, math.pow(2.0, headroom_stops) - 1.0)
    normalized_highlight = np.clip((current_y - 0.68) / 0.32, 0.0, 1.0)
    recovered_y = (
        current_y
        + strength * highlight * normalized_highlight**2 * headroom * 0.24
        + specular * strength * headroom * 0.08
        + (current_y - blurred_y) * highlight * strength * 0.45
    )
    recovered_y += (
        (temporal_median - current_y)
        * temporal_strength
        * agreement
        * np.maximum(highlight, shadow)
        * 0.24
    )
    denoised_y = current_y + (blurred_y - current_y) * shadow * 0.35
    recovered_y = (
        recovered_y * (1.0 - shadow)
        + (denoised_y + shadow * strength * 0.018) * shadow
    )

    ratio = recovered_y / np.maximum(current_y, 0.001)
    recovered = current * ratio[..., None]
    black = current_y < 0.001
    recovered[black] = recovered_y[black, None]
    confidence = np.clip(
        np.maximum(highlight, shadow * 0.65)
        * (0.35 + agreement * 0.65),
        0.0,
        1.0,
    )
    return recovered.astype(np.float32), confidence.astype(np.float32)


def load_model_output(
    directory: Path,
    source_path: Path,
    transfer: str,
    reference_white: float,
) -> np.ndarray:
    model_path = directory / f"{source_path.stem}.npy"
    if not model_path.is_file():
        raise ValueError(f"Missing model output: {model_path}")
    model = np.load(model_path, allow_pickle=False).astype(np.float32)
    if model.ndim != 3 or model.shape[2] < 3:
        raise ValueError(f"Model output must be HxWx3: {model_path}")
    model = model[..., :3]
    if transfer == "pq":
        model = pq_to_relative_linear(model, reference_white)
    elif transfer == "display":
        model = np.power(np.clip(model, 0.0, None), 2.2)
    return model


def cache_filename(frame_index: int) -> str:
    return f"frame_{frame_index:08d}.bwrcache"


def write_cache(
    output_path: Path,
    frame_index: int,
    rgb: np.ndarray,
    confidence: np.ndarray,
) -> None:
    height, width, channels = rgb.shape
    if channels != 3 or confidence.shape != (height, width):
        raise ValueError("Cache image and confidence shapes do not match")
    if not np.isfinite(rgb).all() or not np.isfinite(confidence).all():
        raise ValueError("Cache contains NaN or infinite values")

    payload = np.empty((height, width, 4), dtype="<f2")
    payload[..., :3] = np.clip(rgb, -65504.0, 65504.0).astype("<f2")
    payload[..., 3] = np.clip(confidence, 0.0, 1.0).astype("<f2")
    header = HEADER.pack(MAGIC, VERSION, width, height, frame_index, 0)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(header)
        stream.write(payload.tobytes(order="C"))
    os.replace(temporary, output_path)


def read_header(path: Path) -> dict[str, int]:
    with path.open("rb") as stream:
        raw = stream.read(HEADER.size)
    if len(raw) != HEADER.size:
        raise ValueError(f"Truncated header: {path}")
    magic, version, width, height, frame_index, flags = HEADER.unpack(raw)
    if magic != MAGIC or version != VERSION:
        raise ValueError(f"Unsupported cache format: {path}")
    expected_size = HEADER.size + width * height * 8
    actual_size = path.stat().st_size
    if actual_size != expected_size:
        raise ValueError(
            f"Wrong cache size for {path.name}: {actual_size}, expected {expected_size}"
        )
    return {
        "version": version,
        "width": width,
        "height": height,
        "frame_index": frame_index,
        "flags": flags,
    }


def build_command(args: argparse.Namespace) -> int:
    source_files = collect_images(args.source_dir)
    frames = [load_rgb(path) for path in source_files]
    first_shape = frames[0].shape
    if any(frame.shape != first_shape for frame in frames):
        raise ValueError("All source frames must have the same dimensions")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest_frames = []
    for index, (source_path, source) in enumerate(zip(source_files, frames)):
        frame_index = args.start_frame + index
        native, confidence = native_temporal_recovery(
            frames,
            index,
            args.headroom_stops,
            args.strength,
            args.temporal_strength,
        )
        result = native
        model_used = False
        if args.model_output_dir:
            model = load_model_output(
                args.model_output_dir,
                source_path,
                args.model_transfer,
                args.reference_white,
            )
            if model.shape != source.shape:
                raise ValueError(
                    f"Model output shape {model.shape} does not match {source.shape}"
                )
            recovery_region = np.maximum(
                smoothstep(0.68, 0.99, luma(source)),
                1.0 - smoothstep(0.025, 0.24, luma(source)),
            )
            blend = (
                np.clip(args.model_blend, 0.0, 1.0)
                * recovery_region
                * confidence
            )[..., None]
            result = native * (1.0 - blend) + model * blend
            confidence = np.clip(confidence * (0.45 + recovery_region * 0.55), 0.0, 1.0)
            model_used = True

        output_path = args.output_dir / cache_filename(frame_index)
        write_cache(output_path, frame_index, result, confidence)
        manifest_frames.append(
            {
                "frame_index": frame_index,
                "source": source_path.name,
                "cache": output_path.name,
                "model_used": model_used,
                "mean_confidence": float(np.mean(confidence)),
            }
        )
        print(f"[{index + 1}/{len(frames)}] {output_path.name}")

    height, width, _ = first_shape
    manifest = {
        "format": "Buckswood Radiance Cache",
        "version": VERSION,
        "width": width,
        "height": height,
        "start_frame": args.start_frame,
        "frame_count": len(frames),
        "headroom_stops": args.headroom_stops,
        "model_transfer": args.model_transfer if args.model_output_dir else None,
        "frames": manifest_frames,
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2),
        encoding="utf-8",
    )
    return 0


def validate_command(args: argparse.Namespace) -> int:
    files = sorted(args.cache_dir.glob("frame_*.bwrcache"))
    if not files:
        raise ValueError(f"No cache frames found in {args.cache_dir}")
    headers = [read_header(path) for path in files]
    dimensions = {(item["width"], item["height"]) for item in headers}
    if len(dimensions) != 1:
        raise ValueError("Cache sequence contains mixed dimensions")
    print(
        json.dumps(
            {
                "ok": True,
                "frame_count": len(files),
                "dimensions": list(dimensions)[0],
                "first_frame": headers[0]["frame_index"],
                "last_frame": headers[-1]["frame_index"],
            },
            indent=2,
        )
    )
    return 0


def inspect_command(args: argparse.Namespace) -> int:
    header = read_header(args.cache_file)
    with args.cache_file.open("rb") as stream:
        stream.seek(HEADER.size)
        payload = np.frombuffer(
            stream.read(),
            dtype="<f2",
        ).astype(np.float32).reshape(header["height"], header["width"], 4)
    summary = {
        **header,
        "rgb_min": float(np.min(payload[..., :3])),
        "rgb_max": float(np.max(payload[..., :3])),
        "confidence_min": float(np.min(payload[..., 3])),
        "confidence_max": float(np.max(payload[..., 3])),
        "confidence_mean": float(np.mean(payload[..., 3])),
    }
    print(json.dumps(summary, indent=2))
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    build = subparsers.add_parser("build", help="Build a v2 cache sequence")
    build.add_argument("--source-dir", type=Path, required=True)
    build.add_argument("--output-dir", type=Path, required=True)
    build.add_argument("--model-output-dir", type=Path)
    build.add_argument("--start-frame", type=int, default=0)
    build.add_argument("--headroom-stops", type=float, default=2.0)
    build.add_argument("--strength", type=float, default=0.65)
    build.add_argument("--temporal-strength", type=float, default=0.55)
    build.add_argument("--model-blend", type=float, default=0.85)
    build.add_argument(
        "--model-transfer",
        choices=("pq", "linear", "display"),
        default="pq",
    )
    build.add_argument("--reference-white", type=float, default=203.0)
    build.set_defaults(handler=build_command)

    validate = subparsers.add_parser("validate", help="Validate a cache directory")
    validate.add_argument("--cache-dir", type=Path, required=True)
    validate.set_defaults(handler=validate_command)

    inspect = subparsers.add_parser("inspect", help="Inspect one cache frame")
    inspect.add_argument("--cache-file", type=Path, required=True)
    inspect.set_defaults(handler=inspect_command)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return int(args.handler(args))
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
