# Buckswood AutoGrade Assistant

Shot analysis and conservative auto-grade helper for DaVinci Resolve.

This is the safe way to add "AI assisted grading" around the Buckswood DCTLs:

1. Export a representative still from Resolve.
2. Run this assistant on the still.
3. It creates a shot-specific `.cube` LUT, a before/after preview, a JSON recipe, and a Resolve report.
4. Apply the generated LUT on a separate node, then optionally add the Buckswood DCTL look after it.

The important design choice: the vision/analysis layer suggests bounded parameters. It does not hallucinate a grade or push every shot into the same look.

## Quick Start

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AutoGrade_Assistant
python3 scripts/buckswood_autograde.py /path/to/exported_still.jpg --style photoreal-ai --strength 0.58
```

Outputs are written to:

```text
/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AutoGrade_Assistant/output
```

Copy the generated `.cube` LUT into Resolve's LUT folder or run:

```bash
cp output/*.cube "/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/"
```

Then in Resolve:

1. Refresh LUTs or restart Resolve.
2. Add a new serial node.
3. Apply the generated `Buckswood_AutoGrade_...cube` LUT.
4. Add `Buckswood_AI_Photorealizer_v01.dctl` before or after it at low-to-medium mix.
5. Use the report's recommended DCTL slider values as the starting point.

## Optional Vision Model Loop

For now the tool works without an API key using image statistics. For stronger image understanding:

1. Open `vision_prompt.md`.
2. Paste the prompt and your exported still into a vision model.
3. Save the returned JSON as `my_shot_vision.json`.
4. Run:

```bash
python3 scripts/buckswood_autograde.py /path/to/exported_still.jpg --vision-json my_shot_vision.json
```

This keeps Resolve stable: the model analyzes the still outside Resolve, and Resolve only receives normal LUT/DCTL controls.

## Recommended Node Tree

```text
01 Input CST / camera transform
02 Buckswood AutoGrade LUT
03 Buckswood AI Photorealizer DCTL, low-to-medium mix
04 Creative Buckswood film / IMAX look, low mix
05 Final trim
```

If the footage already looks decent, keep the AutoGrade node key output around `0.35` to `0.65`. If the result gets worse, lower node key output first before changing the whole look.
