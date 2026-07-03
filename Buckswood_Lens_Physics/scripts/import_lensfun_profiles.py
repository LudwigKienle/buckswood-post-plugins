#!/usr/bin/env python3
import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path


def text_of(parent, tag, default=""):
    direct = []
    translated = []
    for child in parent.findall(tag):
        if child.text is None:
            continue
        value = child.text.strip()
        if not value:
            continue
        if "lang" in child.attrib:
            translated.append(value)
        else:
            direct.append(value)
    if direct:
        return direct[0]
    if translated:
        return translated[0]
    return default


def all_text(parent, tag):
    values = []
    for child in parent.findall(tag):
        if child.text and child.text.strip():
            value = child.text.strip()
            if value not in values:
                values.append(value)
    return values


def float_or_none(value):
    if value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def numeric_attrs(element):
    out = {}
    for key, value in sorted(element.attrib.items()):
        if key == "model":
            out[key] = value
            continue
        parsed = float_or_none(value)
        out[key] = parsed if parsed is not None else value
    return out


def parse_lens(lens, source_file):
    maker = text_of(lens, "maker")
    model = text_of(lens, "model")
    if not maker or not model:
        return None

    calibration = lens.find("calibration")
    distortions = []
    tcas = []
    vignettings = []
    crops = []
    if calibration is not None:
        distortions = [numeric_attrs(item) for item in calibration.findall("distortion")]
        tcas = [numeric_attrs(item) for item in calibration.findall("tca")]
        vignettings = [numeric_attrs(item) for item in calibration.findall("vignetting")]
        crops = [numeric_attrs(item) for item in calibration.findall("crop")]

    focal_values = []
    for item in lens.findall("focal"):
        focal = float_or_none(item.attrib.get("value"))
        if focal is not None:
            focal_values.append(focal)
    for group in (distortions, tcas, vignettings, crops):
        for item in group:
            focal = item.get("focal")
            if isinstance(focal, (int, float)) and focal not in focal_values:
                focal_values.append(float(focal))

    aperture_values = []
    for item in lens.findall("aperture"):
        aperture = float_or_none(item.attrib.get("value"))
        if aperture is not None:
            aperture_values.append(aperture)
    for item in vignettings:
        aperture = item.get("aperture")
        if isinstance(aperture, (int, float)) and aperture not in aperture_values:
            aperture_values.append(float(aperture))

    cropfactor = float_or_none(text_of(lens, "cropfactor"))
    profile_id = f"{maker}::{model}::{','.join(all_text(lens, 'mount'))}::{cropfactor or ''}"

    return {
        "id": profile_id,
        "maker": maker,
        "model": model,
        "mounts": all_text(lens, "mount"),
        "cropfactor": cropfactor,
        "focals_mm": sorted(focal_values),
        "apertures": sorted(aperture_values),
        "calibration": {
            "distortion": distortions,
            "tca": tcas,
            "vignetting": vignettings,
            "crop": crops,
        },
        "capabilities": {
            "distortion": bool(distortions),
            "tca": bool(tcas),
            "vignetting": bool(vignettings),
            "crop": bool(crops),
        },
        "source_file": source_file,
    }


def import_profiles(data_dir):
    db_dir = data_dir / "db"
    profiles = []
    skipped = []
    for xml_path in sorted(db_dir.glob("*.xml")):
        if xml_path.name in {"lensfun-database.dtd", "lensfun-database.xsd"}:
            continue
        try:
            tree = ET.parse(xml_path)
        except ET.ParseError as exc:
            skipped.append({"file": str(xml_path), "error": str(exc)})
            continue
        root = tree.getroot()
        for lens in root.findall("lens"):
            profile = parse_lens(lens, xml_path.name)
            if profile:
                profiles.append(profile)
    return profiles, skipped


def summarize(profiles, skipped, source_commit):
    makers = sorted({p["maker"] for p in profiles})
    capabilities = {
        "distortion": sum(1 for p in profiles if p["capabilities"]["distortion"]),
        "tca": sum(1 for p in profiles if p["capabilities"]["tca"]),
        "vignetting": sum(1 for p in profiles if p["capabilities"]["vignetting"]),
        "crop": sum(1 for p in profiles if p["capabilities"]["crop"]),
    }
    return {
        "source": "Lensfun database",
        "source_url": "https://github.com/lensfun/lensfun",
        "source_commit": source_commit,
        "license": "CC BY-SA 3.0 for Lensfun database",
        "profile_count": len(profiles),
        "maker_count": len(makers),
        "makers": makers,
        "capabilities": capabilities,
        "skipped": skipped,
    }


def main():
    parser = argparse.ArgumentParser(description="Import Lensfun XML profiles into compact Buckswood JSON.")
    parser.add_argument(
        "--data-dir",
        default="assets/lensfun_repo/data",
        help="Path to Lensfun data directory containing db/*.xml",
    )
    parser.add_argument(
        "--out-dir",
        default="assets/lens_profiles",
        help="Output directory for generated profile JSON",
    )
    parser.add_argument("--source-commit", default="", help="Lensfun git commit hash.")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    data_dir = (root / args.data_dir).resolve()
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    profiles, skipped = import_profiles(data_dir)
    profiles.sort(key=lambda p: (p["maker"].lower(), p["model"].lower(), ",".join(p["mounts"]).lower()))

    summary = summarize(profiles, skipped, args.source_commit)

    profiles_path = out_dir / "lensfun_profiles.json"
    summary_path = out_dir / "lensfun_summary.json"
    with profiles_path.open("w", encoding="utf-8") as f:
        json.dump({"summary": summary, "profiles": profiles}, f, indent=2, ensure_ascii=False)
    with summary_path.open("w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)

    print(f"Imported {len(profiles)} Lensfun lens profiles")
    print(f"Profiles: {profiles_path}")
    print(f"Summary:  {summary_path}")
    print("License: CC BY-SA 3.0. Keep attribution with redistributed profile data.")


if __name__ == "__main__":
    main()

