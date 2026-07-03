# Buckswood Post Plugins v2.0.1

Bugfix release for the public v2 beta.

## Main Fix

- Buckswood Lens Physics is updated to v0.4.
- Added Overdrive Edge Guard to reduce artificial halos and bright/dark outlines when Overdrive is pushed.
- CA, high-contrast fringing, bloom/coma spill, and sharpening are now damped automatically on hard object edges while staying active in texture, glow, and lens-character areas.
- Added a regression smoketest for high-overdrive hard-edge halos.

## Included Tools

- Buckswood Fake Diagnostic v2.1
- Buckswood Lens Physics v0.4 Overdrive Edge Guard
- Buckswood AI Photorealizer v0.2

## Assets

- `Buckswood_Resolve_Plugins_v2_macOS.dmg`
- `Buckswood_Resolve_Plugins_v2_macOS.zip`
- `Buckswood_Resolve_Plugins_v2_Windows_Setup.exe`
- `Buckswood_Resolve_Plugins_v2_Windows.zip`
- `Buckswood_Nuke_OFX_v2.zip`
- `Buckswood_Premiere_Native_macOS.zip`
- `Buckswood_Premiere_Native_Windows.zip`
- `Buckswood_Premiere_Plugins_Setup.exe`
- `Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt`

## Notes

- Restart the host app after replacing OFX/Premiere bundles so the host refreshes its plugin cache.
- Windows setup apps are not Authenticode-signed yet, so Windows SmartScreen may warn.
