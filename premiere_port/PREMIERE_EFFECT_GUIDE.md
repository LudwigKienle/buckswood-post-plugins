# Buckswood Premiere Effect Guide

Premiere's native video-filter PiPL exposes number sliders rather than the OFX
choice menus used in Resolve. Set the mode sliders to whole numbers.

## Fake Diagnostic

`View`: 0 Diagnostic Overlay, 1 Problem Matte, 2 Reality Match Assist,
3 Assist with Overlay, 4 Category Colors.

## Film Emulation

`Input Space`: 0 Rec.709, 1 AI Rec.709, 2 Timeline/Scene Linear, 3 S-Log3,
4 LogC3, 5 LogC4, 6 Apple Log, 7 BMD Film Gen 5/DWG.

`Film Stock`: 0 Manual, 1 Clean 35mm, 2 500T, 3 250D, 4 Soft Eterna,
5 16mm Documentary, 6 Bleach Bypass, 7 B/W, 8 AI Deplastic,
9 AI Skin Recovery, 10 AI Dream Grain, 11 AI Clean Commercial,
12 Large Format, 13 Desert Epic, 14 Green Cyber, 15 Gritty Night.

`Print Stock`: 0 Manual, 1 Theater Print, 2 Soft Print, 3 Clean Scan,
4 Warm Show, 5 Cool Silver, 6 AI Soft, 7 Dense Theatrical, 8 B/W Silver.

`Process Mode`: 0 Full, 1 Color Only, 2 Texture Only.

`View`: 0 Final, 1 Grain Matte, 2 Halation/Bloom Matte, 3 Density Map,
4 Temporal Map. The temporal map remains empty in the current-frame Premiere port.

## Frame Director

`Aspect`: 0 2.39:1, 1 2.00:1, 2 1.85:1, 3 16:9, 4 4:3, 5 1:1, 6 9:16.

`View`: 0 Result, 1 Guides, 2 Saliency, 3 Crop Mask, 4 Confidence.

`Framing`: 0 Auto, 1 Center, 2 Left Third, 3 Right Third, 4 Manual.

## Radiance Recover

`View`: 0 Result, 1 Recovery Matte, 2 Clipping Map, 3 Difference.

Keep the sequence and clip in a floating-point workflow to retain recovered
values above display white.

## Temporal Integrity

This Premiere build analyzes spatial outliers in the current frame. It is a
stability fallback, not true neighbor-frame temporal analysis.

`View`: 0 Result, 1 Repair Matte, 2 Difference Overlay, 3 Edge Protection,
4 Confidence, 5 Chroma Risk.

## Look DNA

`Reference`: 0 External `.bwlook` slots, 1 Warm Cinema, 2 Cool Neo,
3 Clean Print.

`Match Mode`: 0 Full Look, 1 Color Only, 2 Tone Only, 3 Texture Only.

`View`: 0 Result, 1 Split, 2 Difference, 3 Confidence, 4 Skin Mask,
5 Semantic Zones, 6 Tone Map, 7 Gamut Warning.

For external references, create these files:

```text
macOS:  ~/Documents/Buckswood/LookDNA/reference_a.bwlook
        ~/Documents/Buckswood/LookDNA/reference_b.bwlook
        ~/Documents/Buckswood/LookDNA/reference_c.bwlook

Windows: %USERPROFILE%\Documents\Buckswood\LookDNA\reference_a.bwlook
         %USERPROFILE%\Documents\Buckswood\LookDNA\reference_b.bwlook
         %USERPROFILE%\Documents\Buckswood\LookDNA\reference_c.bwlook
```

The package contains `look_dna/analyze_reference.py`. Install Pillow once, then
create a profile from a JPEG, PNG or TIFF still:

```bash
python3 -m pip install Pillow
python3 look_dna/analyze_reference.py reference.jpg ~/Documents/Buckswood/LookDNA/reference_a.bwlook
```

After replacing a profile, increment `Reload Token` by one. Reference B/C are
optional and controlled with their mix sliders. If slot A is missing, external
mode safely passes the source look through. Studios with redirected Documents
folders can set `BUCKSWOOD_LOOKDNA_DIR` to a custom profile directory before
starting Premiere.
