# Buckswood AutoGrade Report

## Source

- Image: `/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_YouTube_Giveaway_HeyGen/renders/buckswood_davinci_giveaway_ludwig_thumbnail.jpg`
- Resolution: `1280x720`
- Style: `photoreal-ai`
- Strength: `0.55`

## Generated Files

- LUT: `/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AutoGrade_Assistant/output/Buckswood_AutoGrade_buckswood_davinci_giveaway_ludwig_thumbnail_photoreal-ai_33.cube`
- Preview: `/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AutoGrade_Assistant/output/Buckswood_AutoGrade_buckswood_davinci_giveaway_ludwig_thumbnail_photoreal-ai_preview.jpg`
- Recipe: `/Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AutoGrade_Assistant/output/Buckswood_AutoGrade_buckswood_davinci_giveaway_ludwig_thumbnail_photoreal-ai_recipe.json`

## Analysis Summary

- Median luma: `0.720`
- P05/P95 luma span: `0.742`
- Average saturation: `0.268`
- Neon score: `0.188`
- Highlight clipping risk: `0.867`
- Shadow crushing risk: `0.000`
- Plastic look score: `0.435`

## AutoGrade LUT Values

- Exposure trim: `-0.300 EV`
- Contrast multiplier: `0.920`
- Saturation multiplier: `0.960`
- White-balance gains: R `0.967`, G `1.006`, B `1.034`
- Highlight roll-off: `0.777`
- Gamut guard: `0.594`

## Buckswood AI Photorealizer DCTL Starting Point

- Plastic Reduction: `0.55`
- Skin Realism: `0.72`
- Highlight Realism: `0.8`
- Color Naturalize: `0.5`
- Sensor Texture: `0.23`
- Micro Contrast: `0.18`
- Gamut Guard: `0.59`
- Shadow Depth: `0.32`
- Smoothness Breakup: `0.34`
- Output Mix: `0.62`

## Vision Model Notes

- No external vision notes provided.

## Resolve Node Suggestion

```text
01 Input transform / CST if needed
02 Buckswood AutoGrade LUT
03 Buckswood AI Photorealizer DCTL with the values above
04 Creative film / IMAX look at low mix
05 Final trim
```

If the grade gets worse, reduce the key output gain of node 02 before changing the look. Start around `0.50`.
