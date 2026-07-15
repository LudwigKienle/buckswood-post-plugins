# Buckswood Cinematic Tools v2.0

## Frame Director v2

Frame Director previews a cinematic crop for seven target aspect ratios.
Five-frame analysis reduces unstable framing, while shot-cut sensitivity stops
the virtual camera path from smoothing across edits.

Use `Composition Guides` while positioning the frame and switch back to
`Cinematic Crop` for the clean output. Enable `Subject Lock` and set `Locked
Subject X/Y` when a face or hero object must remain fixed.

## Radiance Recover v2

The native path expands highlight headroom, reconstructs local specular energy,
opens damaged shadows and adds dequantization. It remains available without
Python, a model or a cache.

The optional ML path uses a local HDRTVNet++ AGCM model:

```bash
./scripts/setup_ml_backend.sh
.ml-venv/bin/python scripts/hdrtvnet_plus_adapter.py \
  --input-dir /path/to/frames \
  --output-dir /path/to/model-output
.ml-venv/bin/python scripts/radiance_cache.py build \
  --source-dir /path/to/frames \
  --model-output-dir /path/to/model-output \
  --output-dir /path/to/cache
```

Enable `Use ML Cache` in Resolve and choose that cache directory. Use `ML Cache
Confidence` before judging the result. Cache values are stored as half-float RGB
plus half-float confidence and may safely carry values above 1.0.

## Temporal Integrity v2

Temporal Integrity now uses two previous and two following frames. It repairs
flicker and texture outliers only when the temporal neighborhood agrees.

Check these views before increasing repair strength:

- `Temporal Error Matte`
- `Motion Protection Matte`
- `Repair Confidence Matte`
- `Ghost Risk Matte`

If silhouettes double, increase `Ghost Guard`, `Motion Guard` and `Edge
Protection`.

## Recommended order

1. Fake Diagnostic
2. Temporal Integrity
3. AI Photorealizer
4. Radiance Recover
5. Primary color correction
6. Lens Physics
7. Frame Director

ML reconstruction is plausible, not ground truth. Fully clipped source pixels
do not contain enough information to prove what the original scene contained.
