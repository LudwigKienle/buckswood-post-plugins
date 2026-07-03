# Host Release Matrix

## DaVinci Resolve

Status: stable v2 target.

Release assets:

- `Buckswood_Resolve_Plugins_v2_macOS.dmg`
- `Buckswood_Resolve_Plugins_v2_macOS.zip`
- `Buckswood_Resolve_Plugins_v2_Windows_Setup.exe`
- `Buckswood_Resolve_Plugins_v2_Windows.zip`

Included tools:

- Buckswood Fake Diagnostic v2.1
- Buckswood Lens Physics v0.3 Anti-Ghosting
- Buckswood AI Photorealizer v0.2

## Nuke

Status: experimental OFX host package. macOS bundles are Developer-ID-signed.

Release asset:

- `Buckswood_Nuke_OFX_v2.zip`

Nuke can load OpenFX plug-ins directly, so this package reuses the same OFX cores. DCTL files are not included because DCTL is Resolve-only.

Temporal modes in Fake Diagnostic depend on host frame access. If temporal access is limited in Nuke, use the spatial diagnostic modes first.

## Premiere Pro

Status: native beta port. macOS bundles are Developer-ID-signed; Windows installer is not Authenticode-signed yet.

Release assets:

- `Buckswood_Premiere_Native_macOS.zip`
- `Buckswood_Premiere_Native_Windows.zip`
- `Buckswood_Premiere_Plugins_Setup.exe`

Included tools:

- Buckswood Lens Physics
- Buckswood AI Photorealizer

Fake Diagnostic is not included in the Premiere native package yet. Its temporal and diagnostic UI model needs a dedicated Premiere port pass instead of simply copying the Resolve OFX wrapper.

Premiere builds require the Adobe Premiere Pro C++ SDK. Do not commit the SDK to GitHub.
