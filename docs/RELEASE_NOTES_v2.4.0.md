# Buckswood Post Plugins v2.4.0

## Suite-wide performance update

- Fake Diagnostic v2.2 prepares frame seeds and diagnostic weights once per
  render and reuses a single neighborhood analysis across overlays and assist.
- Film Emulation v2.1 prepares stock, print, exposure, grain, gate-weave, and
  process-stage state once per render.
- Frame Director v2.1 shares its five-frame focus and crop analysis across
  OpenFX render tiles with a bounded, thread-safe cache.
- Radiance Recover v2.1 caches complete validated ML cache frames with an LRU
  memory limit instead of reopening and seeking through the file per tile.
- Temporal Integrity v2.1 prepares thresholds and guard constants once per
  render.
- Look DNA v2.2 shares source, temporal, reference, global-match, and spatial
  analysis across render tiles with a bounded, thread-safe cache.
- Premiere Pro uses the same prepared core states for Fake Diagnostic, Film
  Emulation, Frame Director, Radiance Recover, Temporal Integrity, and Look
  DNA.
- Resolve and Nuke continue to use the OpenFX host thread pool. Lens Physics
  v0.5 and AI Photorealizer v0.3 retain the Metal/OpenCL paths introduced in
  v2.3.0.

## Quality and stability

- No FP16 mode, reduced-resolution analysis, proxy samples, or quality switch
  was added.
- Prepared-state parity is exact for Fake Diagnostic, Film Emulation, Radiance
  Recover, and Frame Director.
- The optimized core paths preserve the v2.3.0 Float32 reference output
  bit-for-bit in the automated parity probe.
- Caches are bounded and keyed by plugin instance, frame, source buffers,
  temporal buffers, reference assets, controls, image layout, and bit depth.
- CPU fallbacks remain available on every host.

## Packages

- macOS Resolve/OpenFX DMG and ZIP
- Windows Resolve/OpenFX setup EXE and manual ZIP
- Nuke OpenFX package for macOS and Windows
- Premiere Pro native packages for macOS and Windows
