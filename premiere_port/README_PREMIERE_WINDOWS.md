# Buckswood Premiere Native Plugins for Windows

This package contains native 64-bit `.prm` builds for all eight Buckswood
Premiere effects: Fake Diagnostic, Lens Physics, AI Photorealizer, Film
Emulation, Frame Director, Radiance Recover, Temporal Integrity and Look DNA.

## Install

1. Close Premiere Pro.
2. Run `Buckswood_Premiere_Plugins_Setup.exe` as administrator.
3. Restart Premiere Pro.
4. Search for `Buckswood` in the Effects panel.

For the ZIP package, right-click
`native\scripts\install_premiere_plugins_windows.bat` and choose **Run as
administrator**.

Manual install folder:

```text
C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\Buckswood
```

Copy all eight `.prm` files from `dist` into that folder.

## Notes

- These are Premiere-native plugins, not DaVinci Resolve OFX bundles.
- Rendering uses Premiere's 32-bit floating-point BGRA CPU path.
- Temporal Integrity uses a spatial fallback because this filter API only
  provides the current frame.
- Look DNA external references use `.bwlook` files in the user's Documents
  folder. See `PREMIERE_EFFECT_GUIDE.md`.
- The Windows installer is not Authenticode-signed yet.
