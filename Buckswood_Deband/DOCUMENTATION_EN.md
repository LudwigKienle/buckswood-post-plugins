# Buckswood Deband v1.0

## What it does

Buckswood Deband repairs visible contour steps in skies, walls, shadows,
defocused backgrounds, synthetic gradients, and AI-generated footage. It is
not a global blur. The detector looks for weak terrace-like transitions at
three scales and limits reconstruction to regions that still behave like a
continuous gradient.

## Recommended workflow

1. Place the effect before grain, sharpening, and final output transforms when
   possible.
2. Choose the nearest preset.
3. Switch `View` to `Banding Map` and raise `Band Detection` until the unwanted
   contours appear.
4. Check `Protected Detail`. Important text, silhouettes, grain, and texture
   should appear cyan.
5. Return to `Result`, tune `Repair Strength`, then add only enough `Gradient
   Dither` to stop the contour from reforming after export.
6. Use `Output Mix` for the final balance.

## Controls

### Input and preset

- **Preset**: Chooses tuned multipliers for common sources. Sliders remain
  active as trims.
- **Working Space**: Use `Auto / Wide Gamut` for DaVinci Intermediate, log, and
  most grading timelines. Use `Scene-Linear HDR` only when the node receives
  actual linear-light values.
- **Source Precision**: Sets the expected quantization step. Start with 8-bit
  for web/H.264 or generated sources and 10-bit for camera masters.

### Detection and repair

- **Repair Strength**: Mixes reconstructed gradients into detected contours.
- **Band Detection**: Raises sensitivity to weaker false contours.
- **Repair Radius**: Sets the largest contour scale. Larger values help broad
  sky bands but cost more processing time.
- **Edge Protection**: Protects hard edges, typography, and silhouettes.
- **Texture Protection**: Protects grain, skin detail, hair, fabric, and noise.
- **Chroma Repair**: Extends the repair from luminance into broad color bands.

### Dither and output

- **Gradient Dither**: Adds zero-mean high-frequency dither only inside
  detected gradients.
- **Dither Motion**: `Static / Baselight Safe` is frame-independent.
  `Frame-Indexed` changes deterministically with the OFX frame time and does
  not depend on render order.
- **Dither Seed**: Changes the deterministic pattern.
- **View**: Result, Banding Map, Protected Detail, or Difference x12.
- **Output Mix**: Final dry/wet control for Result.

## Presets

- **Balanced**: Safe general-purpose starting point.
- **Subtle 10-bit**: Conservative finishing for camera originals.
- **8-bit Sky Rescue**: Larger radius and stronger dither for smooth skies.
- **AI Footage Gradient Repair**: High protection with medium-scale repair for
  generated gradients and overly smooth surfaces.
- **Heavy Compression Rescue**: Strongest profile for damaged delivery files.
- **Manual**: Uses the controls without preset multipliers.

## Baselight notes

The v1 core is deliberately single-frame and stateless. It does not require
sequential rendering, request neighboring frames, or retain a hidden temporal
cache. This matches Baselight's frame-independent OpenFX render model. Install
the Linux bundle under a directory scanned by Baselight, normally:

```text
/usr/OFX/Plugins/BuckswoodDeband.ofx.bundle
```

The Linux release uses `Contents/Linux-x86-64/BuckswoodDeband.ofx`.

## ML status

The v1 release does not silently bundle a research checkpoint. Several useful
repositories have permissive code licenses, but model provenance, weights,
runtime size, colorspace assumptions, and video stability still need separate
validation. The current Gradient Repair preset targets AI footage with the
deterministic native algorithm. A future neural backend should be optional and
must produce the same result regardless of frame render order.
