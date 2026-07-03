Buckswood Fake Diagnostic v2 for DaVinci Resolve
================================================

Installation:

1. Close DaVinci Resolve.
2. Double-click Buckswood_Fake_Diagnostic_Installer.pkg.
3. Finish the macOS Installer steps.
4. Restart DaVinci Resolve.

Resolve path:

Color Page > OpenFX > Buckswood > Buckswood Fake Diagnostic

Installed file:

/Library/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle

This package installs only the OpenFX plugin. It does not install a DCTL,
because Fake Diagnostic needs image-neighborhood analysis that belongs in OFX.

v2 adds temporal diagnostics. In Resolve, start with Diagnostic Overlay, then
Category Colors, then Temporal Stability Matte or Temporal Difference Overlay.
Use Reality Match Assist lightly at the end.

If Resolve does not show the plugin immediately, restart Resolve once more or
clear the OpenFX plugin cache from Resolve Preferences.
