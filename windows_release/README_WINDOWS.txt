Buckswood Resolve Plugins for Windows
=====================================

Install:

1. Close DaVinci Resolve completely.
2. Right-click Buckswood_Resolve_Plugins_Setup.exe.
3. Choose "Run as administrator".
4. Restart DaVinci Resolve.

Installed locations:

OFX:
C:\Program Files\Common Files\OFX\Plugins

DCTL:
C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT

Resolve:

Color Page > OpenFX > Buckswood Diagnostic > Buckswood Fake Diagnostic
Color Page > OpenFX > Buckswood > Buckswood Lens Physics
Color Page > OpenFX > Buckswood AI > Buckswood AI Photorealizer
Color Page > OpenFX > Buckswood > Buckswood Film Emulation
Color Page > OpenFX > Buckswood > Buckswood Frame Director
Color Page > OpenFX > Buckswood > Buckswood Radiance Recover
Color Page > OpenFX > Buckswood > Buckswood Temporal Integrity
Color Page > OpenFX > Buckswood > Buckswood Look DNA v2.2
Color Page > DCTL > Buckswood_Lens_Physics_v01
Color Page > DCTL > Buckswood_AI_Photorealizer_v01

Included OFX versions:

Buckswood Fake Diagnostic v2.2
Buckswood Lens Physics v0.5 Metal/OpenCL Performance
Buckswood AI Photorealizer v0.3 Metal/OpenCL Performance
Buckswood Film Emulation v2.1
Buckswood Frame Director v2.1
Buckswood Radiance Recover v2.1
Buckswood Temporal Integrity v2.1
Buckswood Look DNA v2.2

Optional ML companion:

The manual ZIP contains `ML_Companion`. Install Python 3, run
`ML_Companion\scripts\setup_ml_backend_windows.bat`, then use
`hdrtvnet_plus_adapter.py` and `radiance_cache.py` to create the cache selected
inside Radiance Recover.

Film Emulation v2 also includes a macOS/Linux companion script for optional
temporal preprocessing. On Windows, use the same Python command from WSL,
Git Bash, or a normal Python install if the frame paths are adjusted.

Look DNA V2 loads up to three JPEG, PNG, and TIFF references directly. The optional local
`analyze_reference.py` tool creates portable `.bwlook` profiles for repeatable
look matching without uploading the source still.

Note:

This Windows installer is not Authenticode-signed yet. Windows SmartScreen may
warn because Windows code-signing needs a separate Windows-compatible certificate.

GPU acceleration:

Lens Physics v0.5 and AI Photorealizer v0.3 use Resolve's OpenCL buffer path
on compatible NVIDIA and AMD drivers. Float32 processing and the full-quality
sampling pipeline are preserved. If Resolve does not enable OpenCL, the plugins
automatically use Resolve's host-managed CPU thread pool.
