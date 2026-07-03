# Buckswood Premiere Native Windows

This package contains native 64-bit Windows `.prm` builds for Adobe Premiere Pro:

- `BuckswoodAIPhotorealizer.prm`
- `BuckswoodLensPhysics.prm`

## Install

Fast install:

1. Close Premiere Pro.
2. Run `Buckswood_Premiere_Plugins_Setup.exe` as administrator.
3. Restart Premiere Pro.
4. In the Effects panel, search for `Buckswood`.

Manual ZIP install:

1. Close Premiere Pro.
2. Unzip `Buckswood_Premiere_Native_Windows.zip`.
3. Right-click `native\scripts\install_premiere_plugins_windows.bat` and choose **Run as administrator**.
4. Restart Premiere Pro.
5. In the Effects panel, search for `Buckswood`.

Manual install path:

```text
C:\Program Files\Adobe\Common\Plugins\7.0\MediaCore\Buckswood
```

Copy both `.prm` files from `dist` into that folder.

## Notes

- These are Premiere Pro native plugin builds, not DaVinci Resolve OFX builds.
- The plugins are built from the same renderer as the macOS Premiere package.
- If Windows blocks the downloaded ZIP, right-click the ZIP, open **Properties**, choose **Unblock**, then extract again.
- If Premiere was open during installation, restart it before checking the Effects panel.
