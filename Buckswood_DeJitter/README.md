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
Color Page > OpenFX > Buckswood > Buckswood DeJitter v1.0
```

## Recommended workflow

1. Set `View` to `Tracking Region`.
2. Place `Track Point` on a sharp, rigid detail with good contrast.
3. Size the region so it contains texture but no independently moving subject.
4. Switch to `Motion Vector` and verify that tracking is stable.
5. Return to `Stabilized Result` and adjust `DeJitter Strength`.

Use a wall edge, sign, window frame, or fixed prop. Avoid faces, hands, water,
screens, motion blur, and large reflections.
