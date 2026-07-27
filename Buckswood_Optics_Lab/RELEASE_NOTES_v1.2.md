# Buckswood Optics Lab v1.2

## Built-in creative asset UI

- `Lens` remains the coherent algorithmic recipe selector.
- `Glass` adds seven built-in aperture characters.
- `Dirt` adds five built-in dust and surface textures.
- `Smudge` adds four independent fingerprint, wipe, and streak textures.
- No selector requires a path, file browser, or external download.
- Legacy path/aperture/dirt parameters stay serialized but hidden. With all new
  selectors Off, existing project selections render as before.
- Native OFX groups guide the lens-anatomy sequence from Lens State & Focus
  through distortion, chromatic aberration, defocus/glass, flaring/bloom,
  vignetting, Dirt/Smudge, and final sensor/output controls.

The paid 157-image Glass library is not redistributed or relabeled. Previously
installed licensed copies remain an optional legacy source.

## Rendering performance and quality

- Float32 macOS renders use Resolve's Metal buffers.
- Metal covers the full optical pipeline and all three texture channels.
- Safe math, Float32, scene-linear HDR, full tap counts, and source alpha are kept.
- The neutral CPU recipe now returns after one source sample.
- Float32 bilinear CPU sampling removes redundant per-tap bounds/address work.

Automated tests compare CPU and Metal output for the default recipe and an
asset-heavy defocus recipe. The observed maximum difference on the validation
machine was `0.00000418`; the test limit is `0.00005000`.

The 960x540 default-recipe benchmark measured `11.32 ms` on the CPU worker path
and `4.19 ms` on Metal (`2.70x`). Results vary by GPU, host, frame size, and node
graph.

## Current limitation

Optics Lab v1.2 advertises Metal on macOS. Windows keeps the optimized CPU
fallback; an Optics Lab OpenCL backend is not included in this version.
