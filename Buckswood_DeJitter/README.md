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
Color Page > OpenFX > Buckswood > Buckswood DeJitter v1.1
```

## Recommended workflow

1. Set `View` to `Tracking Region`.
2. Drag `Track Point` onto a sharp, rigid detail with good contrast.
3. Use `Track Point Fine X/Y` for exact pixel placement. Enable
   `Snap Point to Texture` when the nearby detail is stronger than the click point.
4. Size the region so it contains texture but no independently moving subject.
5. Switch to `Motion Vector` and verify that tracking is stable.
6. Return to `Stabilized Result` and adjust `DeJitter Strength`.

V1.1 uses a single-pass normalized-correlation analysis and skips high-quality
resampling on unchanged frames. These optimizations reduce render work without
changing the selected tracking quality.

Use a wall edge, sign, window frame, or fixed prop. Avoid faces, hands, water,
screens, motion blur, and large reflections.
