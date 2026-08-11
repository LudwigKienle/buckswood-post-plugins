# Buckswood Optics Lab v1.3.1

## Edge artifact fix

- Replaces dry/wet cross-dissolving of geometric lens transforms with coordinate-
  space interpolation.
- Prevents semi-transparent duplicate silhouettes near frame and subject edges when
  `Output Mix` is below 1.0.
- Keeps distortion, focus breathing, and swirl as one continuous image warp.
- Uses the geometry-aligned image as the baseline for bloom, aberration, surface,
  sensor, and output mixing.

## Validation

- Adds a regression test that verifies partial geometry mix against the expected
  single-sample coordinate position.
- CPU and Metal outputs remain within the existing Float32 parity tolerance.
- Existing plugin identifier, controls, presets, project serialization, and licensed
  local asset compatibility remain unchanged.
