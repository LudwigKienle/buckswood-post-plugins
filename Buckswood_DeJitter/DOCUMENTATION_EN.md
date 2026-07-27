# Buckswood DeJitter v1.0

## Purpose

Buckswood DeJitter removes high-frequency micro shake without completely
locking intentional constant camera movement. It analyzes a selected rigid
image region in the previous and next frames.

## Quick start

1. Apply the effect to a clip or Color node.
2. Set `View` to `Tracking Region`.
3. Place `Track Point` on a sharp, static detail with good contrast.
4. Size `Tracking Region Width/Height` around that detail.
5. Use `Motion Vector` to verify a stable track.
6. Return to `Stabilized Result` and adjust `DeJitter Strength`.

## Controls

- `Track Point`: center of the analyzed image region.
- `Tracking Region Width/Height`: tracking mask size relative to the frame.
- `Maximum Jitter`: pixel search radius.
- `Temporal Analysis`: one frame pair is faster; two pairs are steadier.
- `Tracking Quality`: tracking accuracy and processing cost.
- `DeJitter Strength`: amount of calculated correction applied.
- `Long-Term Stability`: weight of the two-frames-away analysis.
- `Maximum Correction`: safety limit against large image jumps.
- `Tracking Confidence Guard`: below this confidence, the source is unchanged.
- `Scene Cut Protection`: higher values reject uncertain matches more strongly.
- `Resampling`: Bicubic is the default; Lanczos 3 is the sharpest.
- `Frame Edge Handling`: Auto Zoom, reflection, or edge extension.
- `Output Mix`: blend between original and stabilized image.

## Diagnostic views

- `Tracking Region`: selected region and tracking center.
- `Motion Vector`: direction and magnitude of the calculated correction.
- `Confidence`: white is high confidence; black is low confidence.
- `Difference`: only the image areas changed by stabilization.

## Good tracking regions

Use fixed edges, signs, window frames, architecture, or textured props. Avoid
faces, hands, hair, water, screens, reflections, and strong motion blur.
