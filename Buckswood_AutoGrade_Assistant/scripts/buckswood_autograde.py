#!/usr/bin/env python3
import argparse
import colorsys
import json
import math
import re
from pathlib import Path
from statistics import mean

from PIL import Image, ImageFilter


LUMA = (0.2627, 0.6780, 0.0593)


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def lerp(a, b, t):
    return a + (b - a) * t


def safe_name(text):
    text = re.sub(r"[^A-Za-z0-9._-]+", "_", text.strip())
    return text.strip("_") or "shot"


def percentile(sorted_values, q):
    if not sorted_values:
        return 0.0
    q = clamp(q, 0.0, 1.0)
    pos = q * (len(sorted_values) - 1)
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return sorted_values[lo]
    frac = pos - lo
    return lerp(sorted_values[lo], sorted_values[hi], frac)


def srgb_luma(rgb):
    return rgb[0] * LUMA[0] + rgb[1] * LUMA[1] + rgb[2] * LUMA[2]


def load_vision_json(path):
    if not path:
        return {}
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data if isinstance(data, dict) else {}


def analyze_image(image_path):
    img = Image.open(image_path).convert("RGB")
    sample = img.copy()
    sample.thumbnail((640, 640))
    pixels = list(sample.getdata())

    lum_values = []
    sats = []
    rs = []
    gs = []
    bs = []
    skin_count = 0
    neon_count = 0
    high_clip = 0
    low_clip = 0

    for r8, g8, b8 in pixels:
        r = r8 / 255.0
        g = g8 / 255.0
        b = b8 / 255.0
        rs.append(r)
        gs.append(g)
        bs.append(b)
        lum = srgb_luma((r, g, b))
        lum_values.append(lum)
        h, s, v = colorsys.rgb_to_hsv(r, g, b)
        sats.append(s)

        if max(r, g, b) > 0.985:
            high_clip += 1
        if max(r, g, b) < 0.025:
            low_clip += 1
        if s > 0.62 and lum > 0.38:
            neon_count += 1
        if r > g > b and 0.18 < lum < 0.82 and 0.04 < (r - g) < 0.24 and 0.02 < (g - b) < 0.22:
            skin_count += 1

    lum_values.sort()
    sats_sorted = sorted(sats)
    total = max(1, len(pixels))

    gray = sample.convert("L")
    edges = gray.filter(ImageFilter.FIND_EDGES)
    edge_pixels = list(edges.getdata())
    edge_score = mean(edge_pixels) / 255.0 if edge_pixels else 0.0

    p01 = percentile(lum_values, 0.01)
    p05 = percentile(lum_values, 0.05)
    p50 = percentile(lum_values, 0.50)
    p95 = percentile(lum_values, 0.95)
    p99 = percentile(lum_values, 0.99)
    span = p95 - p05
    avg_sat = mean(sats) if sats else 0.0
    p90_sat = percentile(sats_sorted, 0.90)
    mean_r = mean(rs)
    mean_g = mean(gs)
    mean_b = mean(bs)
    mean_rgb = (mean_r + mean_g + mean_b) / 3.0

    flat_score = clamp((0.50 - span) / 0.35, 0.0, 1.0)
    harsh_score = clamp((span - 0.78) / 0.22, 0.0, 1.0)
    neon_score = clamp(neon_count / total * 6.0, 0.0, 1.0)
    clip_high_score = clamp(high_clip / total * 20.0 + (p99 - 0.94) * 4.0, 0.0, 1.0)
    crush_score = clamp(low_clip / total * 20.0 + (0.035 - p01) * 10.0, 0.0, 1.0)
    smooth_score = clamp((0.075 - edge_score) / 0.075, 0.0, 1.0)
    plastic_score = clamp(0.30 * neon_score + 0.35 * smooth_score + 0.20 * flat_score + 0.15 * clip_high_score, 0.0, 1.0)

    return {
        "image": str(image_path),
        "width": img.width,
        "height": img.height,
        "luma": {
            "p01": p01,
            "p05": p05,
            "p50": p50,
            "p95": p95,
            "p99": p99,
            "span_p05_p95": span,
        },
        "saturation": {
            "average": avg_sat,
            "p90": p90_sat,
            "neon_score": neon_score,
        },
        "clip": {
            "highlight_score": clip_high_score,
            "shadow_score": crush_score,
            "high_clip_fraction": high_clip / total,
            "low_clip_fraction": low_clip / total,
        },
        "color": {
            "mean_r": mean_r,
            "mean_g": mean_g,
            "mean_b": mean_b,
            "gray_world_r_gain": clamp(mean_rgb / max(mean_r, 0.001), 0.84, 1.18),
            "gray_world_g_gain": clamp(mean_rgb / max(mean_g, 0.001), 0.84, 1.18),
            "gray_world_b_gain": clamp(mean_rgb / max(mean_b, 0.001), 0.84, 1.18),
        },
        "subject": {
            "skin_candidate_fraction": skin_count / total,
            "edge_score": edge_score,
            "smooth_score": smooth_score,
            "plastic_score": plastic_score,
        },
        "scores": {
            "flat_contrast": flat_score,
            "harsh_contrast": harsh_score,
            "plastic_look": plastic_score,
            "neon_color": neon_score,
            "clipped_highlights": clip_high_score,
            "crushed_shadows": crush_score,
        },
    }


def derive_grade(analysis, vision, style, strength):
    scores = dict(analysis["scores"])
    vision_scores = vision.get("scores", {}) if isinstance(vision.get("scores", {}), dict) else {}
    for key, value in vision_scores.items():
        if isinstance(value, (int, float)):
            scores[key] = clamp((scores.get(key, 0.0) * 0.55) + (float(value) * 0.45), 0.0, 1.0)

    luma = analysis["luma"]
    sat = analysis["saturation"]
    color = analysis["color"]
    subject = analysis["subject"]
    limits = vision.get("recommended_limits", {}) if isinstance(vision.get("recommended_limits", {}), dict) else {}

    mid_target = 0.43
    if style == "moody":
        mid_target = 0.38
    elif style == "commercial":
        mid_target = 0.47

    exposure_ev = math.log(max(0.04, mid_target) / max(0.04, luma["p50"]), 2.0) * 0.70
    if scores.get("clipped_highlights", 0.0) > 0.35:
        exposure_ev = min(exposure_ev, 0.03)
    exposure_ev = clamp(exposure_ev, -0.30, 0.34)

    span = max(0.05, luma["span_p05_p95"])
    contrast = clamp(0.68 / span, 0.92, 1.12)
    if scores.get("harsh_contrast", 0.0) > 0.35:
        contrast = min(contrast, 0.98)
    max_contrast_boost = limits.get("max_contrast_boost")
    if isinstance(max_contrast_boost, (int, float)):
        contrast = min(contrast, 1.0 + clamp(float(max_contrast_boost), 0.0, 0.25))

    saturation = 1.0
    if sat["average"] > 0.34 or scores.get("neon_color", 0.0) > 0.25:
        saturation -= 0.10 + scores.get("neon_color", 0.0) * 0.18
    elif sat["average"] < 0.16 and style != "moody":
        saturation += 0.08
    if style == "photoreal-ai":
        saturation -= 0.04
    max_sat_boost = limits.get("max_saturation_boost")
    if isinstance(max_sat_boost, (int, float)):
        saturation = min(saturation, 1.0 + clamp(float(max_sat_boost), 0.0, 0.25))
    saturation = clamp(saturation, 0.84, 1.10)

    wb_strength = 0.32
    if scores.get("green_cast", 0.0) > 0.25 or scores.get("magenta_cast", 0.0) > 0.25:
        wb_strength = 0.45
    r_gain = lerp(1.0, color["gray_world_r_gain"], wb_strength)
    g_gain = lerp(1.0, color["gray_world_g_gain"], wb_strength)
    b_gain = lerp(1.0, color["gray_world_b_gain"], wb_strength)

    highlight_roll = clamp(0.30 + scores.get("clipped_highlights", 0.0) * 0.55, 0.20, 0.88)
    shadow_lift = clamp(scores.get("crushed_shadows", 0.0) * 0.040, 0.0, 0.045)
    shadow_depth = clamp((1.0 - scores.get("crushed_shadows", 0.0)) * 0.055 + scores.get("flat_contrast", 0.0) * 0.040, 0.0, 0.095)
    gamut_guard = clamp(0.45 + scores.get("neon_color", 0.0) * 0.42 + scores.get("plastic_look", 0.0) * 0.15, 0.35, 0.92)

    dctl = {
        "Plastic Reduction": round(clamp(0.35 + scores.get("plastic_look", 0.0) * 0.45, 0.20, 0.82), 2),
        "Skin Realism": round(clamp(0.42 + min(subject["skin_candidate_fraction"] * 8.0, 0.30), 0.35, 0.72), 2),
        "Highlight Realism": round(clamp(0.45 + scores.get("clipped_highlights", 0.0) * 0.40, 0.35, 0.88), 2),
        "Color Naturalize": round(clamp(0.36 + scores.get("neon_color", 0.0) * 0.34 + scores.get("plastic_look", 0.0) * 0.18, 0.28, 0.86), 2),
        "Sensor Texture": round(clamp(0.14 + scores.get("plastic_look", 0.0) * 0.20, 0.08, 0.38), 2),
        "Micro Contrast": round(clamp(0.18 + scores.get("flat_contrast", 0.0) * 0.34 - scores.get("harsh_contrast", 0.0) * 0.12, 0.08, 0.55), 2),
        "Gamut Guard": round(gamut_guard, 2),
        "Shadow Depth": round(clamp(0.18 + shadow_depth * 2.6, 0.12, 0.42), 2),
        "Smoothness Breakup": round(clamp(0.20 + scores.get("plastic_look", 0.0) * 0.32, 0.12, 0.58), 2),
        "Output Mix": round(clamp(0.48 + strength * 0.26, 0.38, 0.76), 2),
    }

    return {
        "style": style,
        "strength": strength,
        "exposure_ev": exposure_ev,
        "contrast": contrast,
        "saturation": saturation,
        "r_gain": r_gain,
        "g_gain": g_gain,
        "b_gain": b_gain,
        "highlight_roll": highlight_roll,
        "shadow_lift": shadow_lift,
        "shadow_depth": shadow_depth,
        "gamut_guard": gamut_guard,
        "scores": scores,
        "dctl_recommendations": dctl,
        "vision_notes": vision.get("notes", []),
    }


def soft_clip(value, knee, amount):
    if value <= knee:
        return value
    over = value - knee
    return knee + over / (1.0 + over * amount * 8.0)


def apply_grade_rgb(rgb, grade):
    dry = tuple(clamp(c, 0.0, 1.0) for c in rgb)
    r, g, b = dry

    exposure = 2.0 ** grade["exposure_ev"]
    r *= exposure * grade["r_gain"]
    g *= exposure * grade["g_gain"]
    b *= exposure * grade["b_gain"]

    lum = srgb_luma((r, g, b))
    chroma = abs(r - lum) + abs(g - lum) + abs(b - lum)
    neon = clamp((chroma - 0.28) / 0.48, 0.0, 1.0)
    chroma_scale = 1.0 - neon * grade["gamut_guard"] * 0.34
    r = lum + (r - lum) * chroma_scale
    g = lum + (g - lum) * chroma_scale
    b = lum + (b - lum) * chroma_scale

    lum = srgb_luma((r, g, b))
    pivot = 0.42
    shaped_lum = pivot + (lum - pivot) * grade["contrast"]
    delta = shaped_lum - lum
    r += delta
    g += delta
    b += delta

    shadow_mask = clamp((0.38 - shaped_lum) / 0.38, 0.0, 1.0)
    r += shadow_mask * grade["shadow_lift"]
    g += shadow_mask * grade["shadow_lift"]
    b += shadow_mask * grade["shadow_lift"]
    r *= 1.0 - shadow_mask * grade["shadow_depth"]
    g *= 1.0 - shadow_mask * grade["shadow_depth"] * 0.92
    b *= 1.0 - shadow_mask * grade["shadow_depth"] * 0.80

    roll = grade["highlight_roll"]
    r = soft_clip(r, 0.78, roll)
    g = soft_clip(g, 0.78, roll)
    b = soft_clip(b, 0.78, roll)

    lum = srgb_luma((r, g, b))
    r = lum + (r - lum) * grade["saturation"]
    g = lum + (g - lum) * grade["saturation"]
    b = lum + (b - lum) * grade["saturation"]

    mix = grade["strength"]
    out = (
        lerp(dry[0], clamp(r, 0.0, 1.0), mix),
        lerp(dry[1], clamp(g, 0.0, 1.0), mix),
        lerp(dry[2], clamp(b, 0.0, 1.0), mix),
    )
    return tuple(clamp(c, 0.0, 1.0) for c in out)


def write_cube(path, grade, size):
    with open(path, "w", encoding="utf-8") as f:
        f.write(f'TITLE "Buckswood AutoGrade {size}"\n')
        f.write(f"LUT_3D_SIZE {size}\n")
        f.write("DOMAIN_MIN 0.0 0.0 0.0\n")
        f.write("DOMAIN_MAX 1.0 1.0 1.0\n")
        for b_i in range(size):
            b = b_i / (size - 1)
            for g_i in range(size):
                g = g_i / (size - 1)
                for r_i in range(size):
                    r = r_i / (size - 1)
                    out = apply_grade_rgb((r, g, b), grade)
                    f.write(f"{out[0]:.8f} {out[1]:.8f} {out[2]:.8f}\n")


def write_preview(image_path, path, grade):
    src = Image.open(image_path).convert("RGB")
    preview = src.copy()
    preview.thumbnail((960, 540))
    graded = Image.new("RGB", preview.size)
    src_pixels = list(preview.getdata())
    out_pixels = []
    cache = {}
    for r8, g8, b8 in src_pixels:
        key = (r8, g8, b8)
        cached = cache.get(key)
        if cached is not None:
            out_pixels.append(cached)
            continue
        out = apply_grade_rgb((r8 / 255.0, g8 / 255.0, b8 / 255.0), grade)
        packed = tuple(int(round(clamp(c, 0.0, 1.0) * 255.0)) for c in out)
        cache[key] = packed
        out_pixels.append(packed)
    graded.putdata(out_pixels)

    w, h = preview.size
    canvas = Image.new("RGB", (w * 2, h), (18, 18, 18))
    canvas.paste(preview, (0, 0))
    canvas.paste(graded, (w, 0))
    canvas.save(path, quality=92)


def write_report(path, image_path, analysis, grade, cube_path, preview_path, recipe_path):
    dctl_lines = "\n".join(
        f"- {name}: `{value}`" for name, value in grade["dctl_recommendations"].items()
    )
    notes = grade.get("vision_notes") or []
    notes_text = "\n".join(f"- {n}" for n in notes) if notes else "- No external vision notes provided."

    report = f"""# Buckswood AutoGrade Report

## Source

- Image: `{image_path}`
- Resolution: `{analysis['width']}x{analysis['height']}`
- Style: `{grade['style']}`
- Strength: `{grade['strength']:.2f}`

## Generated Files

- LUT: `{cube_path}`
- Preview: `{preview_path}`
- Recipe: `{recipe_path}`

## Analysis Summary

- Median luma: `{analysis['luma']['p50']:.3f}`
- P05/P95 luma span: `{analysis['luma']['span_p05_p95']:.3f}`
- Average saturation: `{analysis['saturation']['average']:.3f}`
- Neon score: `{analysis['saturation']['neon_score']:.3f}`
- Highlight clipping risk: `{analysis['clip']['highlight_score']:.3f}`
- Shadow crushing risk: `{analysis['clip']['shadow_score']:.3f}`
- Plastic look score: `{analysis['subject']['plastic_score']:.3f}`

## AutoGrade LUT Values

- Exposure trim: `{grade['exposure_ev']:+.3f} EV`
- Contrast multiplier: `{grade['contrast']:.3f}`
- Saturation multiplier: `{grade['saturation']:.3f}`
- White-balance gains: R `{grade['r_gain']:.3f}`, G `{grade['g_gain']:.3f}`, B `{grade['b_gain']:.3f}`
- Highlight roll-off: `{grade['highlight_roll']:.3f}`
- Gamut guard: `{grade['gamut_guard']:.3f}`

## Buckswood AI Photorealizer DCTL Starting Point

{dctl_lines}

## Vision Model Notes

{notes_text}

## Resolve Node Suggestion

```text
01 Input transform / CST if needed
02 Buckswood AutoGrade LUT
03 Buckswood AI Photorealizer DCTL with the values above
04 Creative film / IMAX look at low mix
05 Final trim
```

If the grade gets worse, reduce the key output gain of node 02 before changing the look. Start around `0.50`.
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write(report)


def main():
    parser = argparse.ArgumentParser(description="Buckswood still analysis and safe auto-grade LUT generator.")
    parser.add_argument("image", help="Path to exported still image.")
    parser.add_argument("--out", default=None, help="Output directory. Defaults to ./output beside this project.")
    parser.add_argument("--style", default="photoreal-ai", choices=["natural", "photoreal-ai", "cinematic", "moody", "commercial"])
    parser.add_argument("--strength", type=float, default=0.55, help="Blend strength from 0.0 to 1.0.")
    parser.add_argument("--lut-size", type=int, default=33, choices=[17, 33, 65])
    parser.add_argument("--vision-json", default=None, help="Optional JSON from a vision model using vision_prompt.md.")
    args = parser.parse_args()

    image_path = Path(args.image).expanduser().resolve()
    if not image_path.exists():
        raise SystemExit(f"Image not found: {image_path}")

    project_root = Path(__file__).resolve().parents[1]
    out_dir = Path(args.out).expanduser().resolve() if args.out else project_root / "output"
    out_dir.mkdir(parents=True, exist_ok=True)

    strength = clamp(args.strength, 0.0, 1.0)
    vision = load_vision_json(args.vision_json)
    analysis = analyze_image(image_path)
    grade = derive_grade(analysis, vision, args.style, strength)

    stem = safe_name(image_path.stem)
    cube_path = out_dir / f"Buckswood_AutoGrade_{stem}_{args.style}_{args.lut_size}.cube"
    preview_path = out_dir / f"Buckswood_AutoGrade_{stem}_{args.style}_preview.jpg"
    recipe_path = out_dir / f"Buckswood_AutoGrade_{stem}_{args.style}_recipe.json"
    report_path = out_dir / f"Buckswood_AutoGrade_{stem}_{args.style}_report.md"

    write_cube(cube_path, grade, args.lut_size)
    write_preview(image_path, preview_path, grade)

    recipe = {
        "analysis": analysis,
        "grade": grade,
        "vision": vision,
        "outputs": {
            "cube": str(cube_path),
            "preview": str(preview_path),
            "report": str(report_path),
        },
    }
    with open(recipe_path, "w", encoding="utf-8") as f:
        json.dump(recipe, f, indent=2)

    write_report(report_path, image_path, analysis, grade, cube_path, preview_path, recipe_path)

    print("Buckswood AutoGrade complete")
    print(f"LUT:     {cube_path}")
    print(f"Preview: {preview_path}")
    print(f"Report:  {report_path}")
    print("Tip: In Resolve, start with the LUT node key output around 0.50.")


if __name__ == "__main__":
    main()
