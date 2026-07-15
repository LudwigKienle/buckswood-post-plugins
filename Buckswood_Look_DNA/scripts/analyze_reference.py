#!/usr/bin/env python3
"""Create a compact Buckswood Look DNA profile from a local reference still."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageOps


MAGIC = b"BWLOOK1\0"
QUANTILES = (0.01, 0.05, 0.25, 0.50, 0.75, 0.95, 0.99)
COLOR_SPACES = {
    "timeline": 0,
    "rec709": 1,
    "srgb": 2,
    "linear": 3,
    "logc3": 4,
    "logc4": 5,
    "slog3": 6,
    "applelog": 7,
    "bmdgen5": 8,
}
QUALITY_LIMITS = {"draft": 640, "high": 1280, "ultra": 2048}


def smoothstep(edge0: float, edge1: float, value: np.ndarray) -> np.ndarray:
    amount = np.clip((value - edge0) / max(1e-6, edge1 - edge0), 0.0, 1.0)
    return amount * amount * (3.0 - 2.0 * amount)


def decode_transfer(value: np.ndarray, color_space: int) -> np.ndarray:
    value = np.clip(value, 0.0, 64.0)
    if color_space == 1:
        return np.power(value, 2.4)
    if color_space == 2:
        return np.where(
            value <= 0.04045,
            value / 12.92,
            np.power((value + 0.055) / 1.055, 2.4),
        )
    constants = {
        4: (0.391, 6.65),
        5: (0.278, 7.85),
        6: (0.410557, 7.5),
        7: (0.405, 7.05),
        8: (0.380, 6.45),
    }
    if color_space in constants:
        offset, scale = constants[color_space]
        return np.exp2((value - offset) * scale) * 0.18
    return value


def load_working_image(path: Path, color_space: int, quality: str) -> tuple[np.ndarray, np.ndarray]:
    with Image.open(path) as source:
        image = ImageOps.exif_transpose(source).convert("RGBA")
        limit = QUALITY_LIMITS[quality]
        if max(image.size) > limit:
            scale = limit / max(image.size)
            image = image.resize(
                (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
                Image.Resampling.LANCZOS,
            )
        rgba = np.asarray(image, dtype=np.float32) / 255.0
    alpha = rgba[..., 3]
    linear = decode_transfer(rgba[..., :3], color_space)
    working = np.log2(1.0 + 16.0 * np.maximum(linear, 0.0)) / math.log2(17.0)
    return working, alpha


def hue_and_saturation(rgb: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    maximum = np.max(rgb, axis=-1)
    minimum = np.min(rgb, axis=-1)
    delta = maximum - minimum
    saturation = np.where(maximum > 1e-6, delta / np.maximum(maximum, 1e-6), 0.0)
    hue = np.zeros_like(maximum)
    valid = delta > 1e-6
    red = valid & (rgb[..., 0] >= rgb[..., 1]) & (rgb[..., 0] >= rgb[..., 2])
    green = valid & ~red & (rgb[..., 1] >= rgb[..., 2])
    blue = valid & ~red & ~green
    hue[red] = 60.0 * np.mod((rgb[..., 1][red] - rgb[..., 2][red]) / delta[red], 6.0)
    hue[green] = 60.0 * ((rgb[..., 2][green] - rgb[..., 0][green]) / delta[green] + 2.0)
    hue[blue] = 60.0 * ((rgb[..., 0][blue] - rgb[..., 1][blue]) / delta[blue] + 4.0)
    hue = np.where(hue < 0.0, hue + 360.0, hue)
    return hue, saturation


def semantic_masks(
    rgb: np.ndarray,
    luma: np.ndarray,
    hue: np.ndarray,
    saturation: np.ndarray,
    quantiles: np.ndarray,
) -> list[np.ndarray]:
    skin_hue = smoothstep(350.0, 360.0, hue) + (
        1.0 - smoothstep(18.0, 62.0, hue)
    ) * smoothstep(0.0, 18.0, hue)
    skin = np.clip(
        skin_hue
        * smoothstep(0.055, 0.16, saturation)
        * (1.0 - smoothstep(0.72, 0.96, saturation))
        * smoothstep(0.07, 0.18, luma)
        * (1.0 - smoothstep(0.88, 1.08, luma))
        * smoothstep(-0.02, 0.08, rgb[..., 0] - rgb[..., 2]),
        0.0,
        1.0,
    )
    color_presence = smoothstep(0.04, 0.20, saturation)
    sky = np.clip(
        smoothstep(165.0, 195.0, hue)
        * (1.0 - smoothstep(245.0, 275.0, hue))
        * color_presence
        * smoothstep(0.16, 0.42, luma),
        0.0,
        1.0,
    )
    foliage = np.clip(
        smoothstep(52.0, 82.0, hue)
        * (1.0 - smoothstep(150.0, 182.0, hue))
        * color_presence
        * smoothstep(0.08, 0.30, luma),
        0.0,
        1.0,
    )
    warm_hue = (1.0 - smoothstep(32.0, 72.0, hue)) + smoothstep(330.0, 360.0, hue)
    warm = np.clip(
        warm_hue
        * color_presence
        * smoothstep(float(quantiles[4]), max(float(quantiles[4]) + 0.02, float(quantiles[5])), luma),
        0.0,
        1.0,
    )
    shadow = 1.0 - smoothstep(
        float(quantiles[2]) * 0.55,
        max(float(quantiles[2]), 0.04),
        luma,
    )
    highlight = smoothstep(
        float(quantiles[4]),
        max(float(quantiles[4]) + 0.02, float(quantiles[5])),
        luma,
    )
    return [skin, sky, foliage, warm, shadow, highlight]


def analyze(path: Path, color_space: int, quality: str) -> tuple[list[float], dict[str, object]]:
    rgb, alpha = load_working_image(path, color_space, quality)
    valid = alpha > 0.02
    if int(np.count_nonzero(valid)) < 16:
        raise ValueError("Reference image does not contain enough visible pixels")

    luma = 0.2627 * rgb[..., 0] + 0.6780 * rgb[..., 1] + 0.0593 * rgb[..., 2]
    u = rgb[..., 0] - luma
    v = rgb[..., 2] - luma
    hue, saturation = hue_and_saturation(rgb)
    quantiles = np.quantile(luma[valid], QUANTILES).astype(np.float32)

    radius = max(2, min(rgb.shape[0], rgb.shape[1]) // 180)
    wide_average = (
        np.roll(luma, radius, axis=0)
        + np.roll(luma, -radius, axis=0)
        + np.roll(luma, radius, axis=1)
        + np.roll(luma, -radius, axis=1)
    ) * 0.25
    near_average = (
        np.roll(luma, 1, axis=0)
        + np.roll(luma, -1, axis=0)
        + np.roll(luma, 1, axis=1)
        + np.roll(luma, -1, axis=1)
    ) * 0.25
    near_gradient = (
        np.abs(np.roll(luma, -1, axis=1) - np.roll(luma, 1, axis=1))
        + np.abs(np.roll(luma, -1, axis=0) - np.roll(luma, 1, axis=0))
    ) * 0.25
    local_contrast = np.abs(luma - wide_average)
    grain = np.maximum(0.0, np.abs(luma - near_average) - near_gradient * 0.42)

    mean_u = float(np.mean(u[valid]))
    mean_v = float(np.mean(v[valid]))
    centered_u = u[valid] - mean_u
    centered_v = v[valid] - mean_v
    cov_uu = max(0.00002, float(np.mean(centered_u * centered_u)))
    cov_uv = float(np.mean(centered_u * centered_v))
    cov_vv = max(0.00002, float(np.mean(centered_v * centered_v)))

    zones: list[tuple[float, float, float]] = []
    for mask in semantic_masks(rgb, luma, hue, saturation, quantiles):
        weights = np.where(valid, mask, 0.0)
        weight_sum = float(np.sum(weights))
        normalized_weight = weight_sum / max(1.0, float(np.count_nonzero(valid)))
        if weight_sum > 1e-4:
            zones.append(
                (
                    float(np.sum(u * weights) / weight_sum),
                    float(np.sum(v * weights) / weight_sum),
                    normalized_weight,
                )
            )
        else:
            zones.append((0.0, 0.0, 0.0))

    values = [float(value) for value in quantiles]
    values.extend(
        [
            float(np.mean(luma[valid])),
            float(np.std(luma[valid])),
            float(np.mean(saturation[valid])),
            float(np.mean(local_contrast[valid])),
            float(np.mean(grain[valid])),
            mean_u,
            mean_v,
            cov_uu,
            cov_uv,
            cov_vv,
        ]
    )
    for zone in zones:
        values.extend(zone)
    if len(values) != 35:
        raise RuntimeError(f"Internal profile field count is {len(values)}, expected 35")

    report = {
        "format": "BWLOOK1",
        "reference": str(path),
        "color_space": color_space,
        "quality": quality,
        "sample_count": int(np.count_nonzero(valid)),
        "luma_quantiles": values[:7],
        "luma_mean": values[7],
        "luma_std": values[8],
        "saturation_mean": values[9],
        "local_contrast": values[10],
        "grain_estimate": values[11],
        "semantic_backend": "native-zone-v1",
        "zones": {
            name: {"mean_u": zone[0], "mean_v": zone[1], "weight": zone[2]}
            for name, zone in zip(
                ("skin", "sky", "foliage", "warm_light", "shadow", "highlight"),
                zones,
            )
        },
    }
    return values, report


def write_profile(path: Path, values: list[float], sample_count: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(MAGIC)
        stream.write(struct.pack("<III", 1, len(values), sample_count))
        stream.write(struct.pack(f"<{len(values)}f", *values))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="Reference still: JPEG, PNG, TIFF, or another Pillow format")
    parser.add_argument("output", type=Path, help="Output .bwlook profile")
    parser.add_argument(
        "--reference-space",
        choices=sorted(COLOR_SPACES),
        default="srgb",
        help="Color encoding of the reference still",
    )
    parser.add_argument("--quality", choices=sorted(QUALITY_LIMITS), default="high")
    parser.add_argument("--report", type=Path, help="Optional human-readable JSON analysis report")
    args = parser.parse_args()

    values, report = analyze(args.reference, COLOR_SPACES[args.reference_space], args.quality)
    write_profile(args.output, values, int(report["sample_count"]))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Created {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
