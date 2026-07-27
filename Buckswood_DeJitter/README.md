# Buckswood DeJitter

Temporal OpenFX micro-jitter removal for DaVinci Resolve.

The effect tracks a user-selected image region across the previous and next
frames. Symmetric frame comparison rejects constant camera movement and
isolates high-frequency shake. Low-confidence tracks and scene cuts fall back
to the unchanged source frame.

## Build and test

```bash
make -C Buckswood_DeJitter all
```

## Install for the current macOS user

```bash
make -C Buckswood_DeJitter install-user
```

Restart DaVinci Resolve, then open:

```text
Color Page > OpenFX > Buckswood > Buckswood DeJitter v1.2
```

## Recommended workflow

1. Enable `Show Viewer Tracking Area`.
2. Drag the center cross or anywhere inside the rectangle onto a sharp, rigid
   detail with good contrast.
3. Drag a side handle to change width or height. Drag a corner handle to resize
   both dimensions.
4. Use `Track Point Fine X/Y` for exact pixel placement. Enable
   `Snap Point to Texture` when the nearby detail is stronger than the click point.
5. Switch to `Motion Vector` and verify that tracking is stable.
6. Return to `Stabilized Result` and adjust `DeJitter Strength`.

The rectangle is an analysis region, not an effect mask. DeJitter stabilizes the
full frame. Use a Resolve node mask when only part of the image should be
affected.

V1.2 keeps the single-pass normalized-correlation analysis and skips high-quality
resampling on unchanged frames. These optimizations reduce render work without
changing the selected tracking quality.

Use a wall edge, sign, window frame, or fixed prop. Avoid faces, hands, water,
screens, motion blur, and large reflections.
