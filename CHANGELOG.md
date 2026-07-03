# Changelog

## v2.0.1 - 2026-07-03

- Updated `Buckswood Lens Physics` to v0.4 with an overdrive-aware edge guard for high-contrast silhouettes.
- Added `Overdrive Edge Guard`, which automatically reduces CA, fringing, bloom/coma spill, and sharpening on hard edges while preserving the stronger lens look in texture and highlights.
- Added a regression smoketest for high-overdrive hard-edge halos.
- Updated the shared Lens Physics core default used by the Premiere native port.

## v2 - 2026-07-03

- Added `Buckswood Fake Diagnostic v2.1`.
- Updated `Buckswood Lens Physics` to v0.3 with anti-ghosting and stronger silhouette protection.
- Updated `Buckswood AI Photorealizer` to v0.2 with safer adjustment-layer handling.
- Added unified macOS release DMG/ZIP.
- Added unified Windows setup EXE and manual ZIP with all three Resolve OFX plugins.
- Added Nuke OFX v2 package with Fake Diagnostic, Lens Physics, and AI Photorealizer.
- Rebuilt Premiere Pro native macOS and Windows packages.

## Earlier Builds

- Initial DCTL film-emulation experiments.
- Initial Resolve OFX builds for Lens Physics and AI Photorealizer.
- Experimental Premiere and Nuke packaging work.
