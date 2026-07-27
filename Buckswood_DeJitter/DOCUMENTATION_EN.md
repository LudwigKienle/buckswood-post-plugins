# Buckswood DeJitter v1.2

## Purpose

Buckswood DeJitter removes high-frequency micro shake without completely
locking intentional constant camera movement. It analyzes a selected rigid
image region in the previous and next frames.

## Quick start

1. Apply the effect to a clip or Color node.
2. Enable `Show Viewer Tracking Area`.
3. Drag the center cross or the rectangle interior onto a sharp, static detail.
4. Drag side handles to change one dimension or corner handles to resize both.
5. Refine the point with `Track Point Fine X/Y`; optionally enable texture
   snapping.
6. Use `Motion Vector` to verify a stable track.
7. Return to `Stabilized Result` and adjust `DeJitter Strength`.

The viewer rectangle selects the region used to estimate motion. It is not a
local effect mask: stabilization is applied to the full frame. Use a Resolve
node mask when the correction should affect only part of the image.

## Controls

- `Track Point`: center of the analyzed image region.
- `Track Point Fine X/Y`: pixel-accurate offset added to the viewer handle.
- `Snap Point to Texture`: searches nearby for a stronger rigid detail.
- `Texture Snap Radius`: maximum distance used by texture snapping.
- `Tracking Region Width/Height`: tracking mask size relative to the frame.
- `Show Viewer Tracking Area`: displays the interactive center and resize
  handles in the Resolve viewer.
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

## V1.2 viewer controls and performance

Normalized correlation now samples each candidate patch once. Frames with no
trusted correction also bypass high-quality resampling. Neither optimization
reduces the selected tracking quality or final interpolation quality.

The v1.2 overlay uses the host-independent OpenFX Draw Suite. It only draws
viewer controls and does not become part of the rendered image.

## Diagnostic views

- `Tracking Region`: selected region and tracking center.
- `Motion Vector`: direction and magnitude of the calculated correction.
- `Confidence`: white is high confidence; black is low confidence.
- `Difference`: only the image areas changed by stabilization.

## Good tracking regions

Use fixed edges, signs, window frames, architecture, or textured props. Avoid
faces, hands, hair, water, screens, reflections, and strong motion blur.
