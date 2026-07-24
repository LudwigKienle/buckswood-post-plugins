# Buckswood Post Plugins v2.3.0

## Performance release

- Lens Physics v0.5 adds full-quality Float32 Metal rendering on macOS and
  OpenCL rendering on Windows.
- AI Photorealizer v0.3 adds full-quality Float32 Metal rendering on macOS and
  OpenCL rendering on Windows.
- Metal pipelines and OpenCL programs are cached per GPU device/context.
- Lens Physics prepares lens models and frame constants once per render.
- Fake Diagnostic, Film Emulation, Cinematic Tools, and Look DNA now use the
  OpenFX host thread pool instead of creating operating-system threads for
  every render.
- Adjustment Layer Guard remains active on GPU through an asynchronous
  GPU-side guard pass.
- CPU fallbacks and byte-image support remain available.

## Validation

- AI Photorealizer CPU/Metal maximum difference: `0.0000000`
- Lens Physics CPU/Metal maximum difference: `0.00000394`
- Measured M3 Ultra 1080p speedup range: 2.25x-4.10x Photorealizer,
  4.06x-6.50x Lens Physics
- Windows Resolve installer builds with the OpenCL runtime loader and requires
  no bundled OpenCL DLL.
