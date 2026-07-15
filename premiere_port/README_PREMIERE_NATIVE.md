# Buckswood Premiere Native Plugins for macOS

This package contains eight native Adobe Premiere Pro video filters:

- Buckswood Fake Diagnostic
- Buckswood Lens Physics
- Buckswood AI Photorealizer
- Buckswood Film Emulation
- Buckswood Frame Director
- Buckswood Radiance Recover
- Buckswood Temporal Integrity
- Buckswood Look DNA

They use Premiere's native C++ `xFilter` entry point and expose animatable
Effect Controls through PiPL resources. Every effect processes Premiere's
32-bit floating-point BGRA frames on the CPU.

## Install

1. Close Premiere Pro.
2. Double-click `native/scripts/install_premiere_plugins.command`.
3. Enter the macOS administrator password when requested.
4. Restart Premiere Pro and search for `Buckswood` in the Effects panel.

Manual install folder:

```text
/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/
```

## Build

```bash
ARCHS="arm64 x86_64" native/scripts/build_premiere_plugins.sh "/path/to/Premiere Pro C++ SDK"
```

The packaged release is `release/Buckswood_Premiere_Native_macOS.zip`.
Individual bundles and ZIP files are written to `dist/`.

## Premiere-Specific Behavior

- Fake Diagnostic includes all spatial views and Reality Match Assist. Its
  Resolve-only temporal views are omitted because this Premiere filter API
  supplies the current frame only.
- Film Emulation includes the negative/print pipeline, AI stocks, grain,
  halation and bloom. Temporal reconstruction stays disabled in this host.
- Frame Director performs current-frame saliency analysis, crop guidance,
  subject lock and manual framing. Cut-aware temporal smoothing stays disabled.
- Radiance Recover keeps floating-point headroom and all spatial recovery tools.
- Temporal Integrity uses a clearly documented spatial stability fallback in
  Premiere. It does not pretend to inspect adjacent frames.
- Look DNA performs real profile-based look matching. External references use
  three `.bwlook` slots because this PiPL filter API has no path picker.

See `PREMIERE_EFFECT_GUIDE.md` for numeric mode maps and Look DNA profile setup.
