# Changelog

## v2 - 2026-07-03

- Fixed macOS installers failing with "Installation failed" (PKInstallErrorDomain error 112): the postinstall script scanned all of `/Users` recursively for Resolve's OFX plugin cache and hit the 600-second installer script timeout on large disks. The cache cleanup now uses a direct path glob.
- Removed an ad-hoc re-sign in the Lens Physics and AI Photorealizer postinstall scripts that replaced the notarized Developer ID signature after install.
- Lens Physics and AI Photorealizer installer builds now sign, notarize, and staple like Fake Diagnostic.
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
