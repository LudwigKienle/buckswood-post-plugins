# Buckswood Optics Lab v1.3

## Lens system

- Adds twelve independent lens recipes without changing preset indices 0-8.
- Adds a shared 3-16 blade procedural iris with roundness and rotation trims.
- Corrects foreground cat-eye displacement and odd-blade bokeh orientation.
- Adds f-stop-dependent diffraction and deterministic spoke unevenness.
- Adds scene units and scene scale for production focus-distance metadata.

## Workflow and performance

- Adds Preview and Full render quality. Full preserves the v1.2 sampling path.
- Adds seven stage switches; disabled stages are removed before rendering.
- Preview reduces only defocus, glow, coma, and maximum star-spoke samples.
- Keeps Float32 scene-linear/HDR processing with no FP16 quality reduction.
- Keeps Metal on macOS and the optimized CPU fallback on Windows.

## Compatibility

- Uses the existing `com.buckswood.optics.lab` plugin identifier.
- Existing controls, preset indices, built-in assets, and hidden legacy asset values
  remain serialized under their original names.
- No proprietary Glass engine, encrypted module, or paid image asset is included in
  the public package. Licensed local assets remain available through the optional
  private installer.
