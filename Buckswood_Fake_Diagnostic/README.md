# Buckswood Fake Diagnostic

`Buckswood Fake Diagnostic v2` is an OpenFX plugin for spotting and gently correcting common "feels fake" signals in AI, CG, and comped footage.

It is designed as a supervisor tool, not a one-click creative look.

## Install

Recommended:

1. Open `Buckswood_Fake_Diagnostic_Installer.dmg`.
2. Run `Buckswood_Fake_Diagnostic_Installer.pkg`.
3. Restart DaVinci Resolve.

macOS:

```text
~/Library/OFX/Plugins
```

Copy `BuckswoodFakeDiagnostic.ofx.bundle` into that folder and restart DaVinci Resolve.

Windows:

```text
C:\Program Files\Common Files\OFX\Plugins
```

Copy `BuckswoodFakeDiagnostic.ofx.bundle` into that folder and restart DaVinci Resolve.

## View Modes

- `Diagnostic Overlay`: overlays problem categories on the original image.
- `Problem Matte`: shows a grayscale suspicion map.
- `Reality Match Assist`: applies subtle corrective processing.
- `Assist + Overlay`: shows the correction with diagnostic color on top.
- `Category Colors`: false-color map of what the plugin thinks is driving the fake feeling.
- `Temporal Stability Matte`: shows areas with suspicious frame-to-frame stability or flicker risk.
- `Temporal Difference Overlay`: overlays the previous/current/next-frame difference.

## Category Colors

- Magenta: over-smooth / plastic texture.
- Yellow: highlight clipping or synthetic rolloff.
- Cyan: suspiciously hard edges.
- Red: gamut, saturation, or grade mismatch risk.
- Blue: missing sensor texture.
- Green: temporal movement or flicker risk.

## Current Limitations

v2 is still intentionally conservative. It can detect image-level proxies for plastic smoothness, highlight rolloff, edge harshness, gamut pressure, texture mismatch, and temporal instability by reading neighboring frames when the host provides them.

Matte-vs-plate diagnosis still requires a future multi-input version.
