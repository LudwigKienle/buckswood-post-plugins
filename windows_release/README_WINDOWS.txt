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
Color Page > DCTL > Buckswood_Lens_Physics_v01
Color Page > DCTL > Buckswood_AI_Photorealizer_v01

Included OFX versions:

Buckswood Fake Diagnostic v2.1
Buckswood Lens Physics v0.4 Overdrive Edge Guard
Buckswood AI Photorealizer v0.2
Buckswood Film Emulation v2.0
Buckswood Frame Director v2.0
Buckswood Radiance Recover v2.0
Buckswood Temporal Integrity v2.0

Optional ML companion:

The manual ZIP contains `ML_Companion`. Install Python 3, run
`ML_Companion\scripts\setup_ml_backend_windows.bat`, then use
`hdrtvnet_plus_adapter.py` and `radiance_cache.py` to create the cache selected
inside Radiance Recover.

Film Emulation v2 also includes a macOS/Linux companion script for optional
temporal preprocessing. On Windows, use the same Python command from WSL,
Git Bash, or a normal Python install if the frame paths are adjusted.

Note:

This Windows installer is not Authenticode-signed yet. Windows SmartScreen may
warn because Windows code-signing needs a separate Windows-compatible certificate.
