#!/usr/bin/env python3
"""Run the MIT-licensed HDRTVNet++ AGCM model on an image sequence."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import numpy as np
    import torch
    import torch.nn as nn
    import torch.nn.functional as functional
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Missing ML dependencies. Run scripts/setup_ml_backend.sh first."
    ) from exc


EXTENSIONS = {".png", ".tif", ".tiff", ".jpg", ".jpeg", ".bmp", ".webp"}


def color_block(in_filters: int, out_filters: int, normalization: bool) -> list[nn.Module]:
    layers: list[nn.Module] = [
        nn.Conv2d(in_filters, out_filters, 1, stride=1, padding=0),
        nn.AvgPool2d(3, stride=2, padding=1, count_include_pad=True),
        nn.LeakyReLU(0.2),
    ]
    if normalization:
        layers.append(nn.InstanceNorm2d(out_filters, affine=True))
    return layers


class ColorCondition(nn.Module):
    def __init__(self, output_channels: int = 6) -> None:
        super().__init__()
        self.model = nn.Sequential(
            *color_block(3, 16, True),
            *color_block(16, 32, True),
            *color_block(32, 64, True),
            *color_block(64, 128, True),
            *color_block(128, 128, False),
            nn.Dropout(p=0.5),
            nn.Conv2d(128, output_channels, 1, stride=1, padding=0),
            nn.AdaptiveAvgPool2d(1),
        )

    def forward(self, image: torch.Tensor) -> torch.Tensor:
        return self.model(image)


class ConditionNet(nn.Module):
    def __init__(self, features: int = 64, condition_channels: int = 6) -> None:
        super().__init__()
        self.classifier = ColorCondition(condition_channels)
        self.cond_scale_first = nn.Linear(condition_channels, features)
        self.cond_scale_HR = nn.Linear(condition_channels, features)
        self.cond_scale_last = nn.Linear(condition_channels, 3)
        self.cond_shift_first = nn.Linear(condition_channels, features)
        self.cond_shift_HR = nn.Linear(condition_channels, features)
        self.cond_shift_last = nn.Linear(condition_channels, 3)
        self.conv_first = nn.Conv2d(3, features, 1, 1)
        self.HRconv = nn.Conv2d(features, features, 1, 1)
        self.conv_last = nn.Conv2d(features, 3, 1, 1)

    def condition_features(self, condition: torch.Tensor) -> torch.Tensor:
        return self.classifier(condition).squeeze(3).squeeze(2)

    def apply_mapping(
        self,
        content: torch.Tensor,
        condition_features: torch.Tensor,
    ) -> torch.Tensor:
        scale_first = self.cond_scale_first(condition_features)
        shift_first = self.cond_shift_first(condition_features)
        scale_hr = self.cond_scale_HR(condition_features)
        shift_hr = self.cond_shift_HR(condition_features)
        scale_last = self.cond_scale_last(condition_features)
        shift_last = self.cond_shift_last(condition_features)

        output = self.conv_first(content)
        output = (
            output * scale_first[:, :, None, None]
            + shift_first[:, :, None, None]
            + output
        )
        output = functional.relu(output)
        output = self.HRconv(output)
        output = (
            output * scale_hr[:, :, None, None]
            + shift_hr[:, :, None, None]
            + output
        )
        output = functional.relu(output)
        output = self.conv_last(output)
        return (
            output * scale_last[:, :, None, None]
            + shift_last[:, :, None, None]
            + output
        )


def choose_device(requested: str) -> torch.device:
    if requested != "auto":
        return torch.device(requested)
    if torch.cuda.is_available():
        return torch.device("cuda")
    if hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
        return torch.device("mps")
    return torch.device("cpu")


def load_rgb(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        array = np.asarray(image)
        if array.ndim != 3 or array.shape[2] < 3:
            array = np.asarray(image.convert("RGB"))
        else:
            array = array[..., :3]
    maximum = float(np.iinfo(array.dtype).max) if np.issubdtype(array.dtype, np.integer) else 1.0
    return array.astype(np.float32) / maximum


def run_tiled(
    model: ConditionNet,
    image: np.ndarray,
    device: torch.device,
    tile_size: int,
) -> np.ndarray:
    tensor = torch.from_numpy(image.transpose(2, 0, 1)).unsqueeze(0).to(device)
    condition = functional.interpolate(
        tensor,
        scale_factor=0.25,
        mode="bicubic",
        align_corners=False,
    )
    with torch.inference_mode():
        features = model.condition_features(condition)
        output = torch.empty_like(tensor)
        height, width = image.shape[:2]
        for y in range(0, height, tile_size):
            for x in range(0, width, tile_size):
                tile = tensor[:, :, y:y + tile_size, x:x + tile_size]
                output[:, :, y:y + tile_size, x:x + tile_size] = model.apply_mapping(
                    tile,
                    features,
                )
    return output[0].detach().float().cpu().numpy().transpose(1, 2, 0)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--weights",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "models" / "hdrtvnet_plus" / "AGCM.pth",
    )
    parser.add_argument("--device", default="auto")
    parser.add_argument("--tile-size", type=int, default=512)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    files = sorted(
        path for path in args.input_dir.iterdir()
        if path.is_file() and path.suffix.lower() in EXTENSIONS
    )
    if not files:
        raise SystemExit(f"No images found in {args.input_dir}")
    if not args.weights.is_file():
        raise SystemExit(f"Missing HDRTVNet++ weights: {args.weights}")

    device = choose_device(args.device)
    model = ConditionNet().to(device)
    state = torch.load(args.weights, map_location=device, weights_only=True)
    model.load_state_dict(state, strict=True)
    model.eval()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    print(f"HDRTVNet++ AGCM device: {device}")
    for index, path in enumerate(files):
        image = load_rgb(path)
        result = run_tiled(model, image, device, max(64, args.tile_size))
        output_path = args.output_dir / f"{path.stem}.npy"
        np.save(output_path, result.astype(np.float16), allow_pickle=False)
        print(f"[{index + 1}/{len(files)}] {output_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
