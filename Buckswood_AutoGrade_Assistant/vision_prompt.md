# Vision Model Prompt

Use this with a strong vision model and one exported frame from Resolve.

```text
You are a senior colorist and image-quality analyst. Analyze this frame for a conservative DaVinci Resolve auto-grade. Do not invent a stylized look. The goal is to make the image look more photographic and less plastic while preserving intent.

Return JSON only, no prose.

Use scores from 0.0 to 1.0:

{
  "scores": {
    "plastic_look": 0.0,
    "skin_waxy": 0.0,
    "too_saturated": 0.0,
    "neon_color": 0.0,
    "flat_contrast": 0.0,
    "crushed_shadows": 0.0,
    "clipped_highlights": 0.0,
    "underexposed": 0.0,
    "overexposed": 0.0,
    "green_cast": 0.0,
    "magenta_cast": 0.0,
    "too_warm": 0.0,
    "too_cool": 0.0,
    "good_as_is": 0.0
  },
  "intent": "natural | cinematic | moody | commercial | documentary",
  "notes": [
    "short practical observation"
  ],
  "recommended_limits": {
    "max_saturation_boost": 0.0,
    "max_contrast_boost": 0.0,
    "preserve_skin": true,
    "protect_highlights": true
  }
}
```

After the model returns JSON, save it and pass it to:

```bash
python3 scripts/buckswood_autograde.py /path/to/still.jpg --vision-json /path/to/model_result.json
```

