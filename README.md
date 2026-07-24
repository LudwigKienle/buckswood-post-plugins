# Buckswood Post Plugins

Free cinematic post-production tools for DaVinci Resolve, with native Premiere Pro and experimental Nuke packages.

This repository contains the source for eight Buckswood Resolve tools:

- **Buckswood Fake Diagnostic v2.2** - a supervisor-style diagnostic overlay for AI, CG, and compositing shots that feel fake.
- **Buckswood Lens Physics v0.5** - lossless Metal/OpenCL acceleration, lens-character simulation, overdrive edge protection, chromatic aberration, bloom, coma, vignette, and IMAX/Dune-inspired presets.
- **Buckswood AI Photorealizer v0.3** - lossless Metal/OpenCL acceleration and a conservative de-plastic and photoreal assist pass for AI/CG/phone footage.
- **Buckswood Film Emulation v2.1** - AI-footage-first film finishing with negative/print process controls, texture-only mode, temporal reconstruction, grain, halation, bloom, and printer-light trims.
- **Buckswood Frame Director v2.1** - five-frame, cut-aware composition assistance with Subject Lock.
- **Buckswood Radiance Recover v2.1** - floating-point SDR radiance expansion with an optional HDRTVNet++ cache.
- **Buckswood Temporal Integrity v2.1** - five-frame repair with confidence and ghost-risk protection.
- **Buckswood Look DNA v2.2** - three-reference, spatial and five-frame look matching with native Browse / Load file pickers, tone, palette, semantic-region, texture, identity, gamut, and cut protection.

The primary release target is DaVinci Resolve/OpenFX. Premiere Pro and Nuke builds are packaged as separate host downloads.

Experimental adjacent work is kept in separate folders:

- `premiere_port/` - native Premiere builds for all eight effects.
- `Buckswood_AutoGrade_Assistant/` - companion script concept for AI-assisted grading recipes.

## Host Status

| Host | Package | Status |
| --- | --- | --- |
| DaVinci Resolve | OpenFX + DCTL installers | Stable v2 target |
| Nuke | OpenFX bundle ZIP | Experimental, uses the same OFX cores |
| Premiere Pro | Native `.prm` / `.bundle` plugins | Beta, all eight effects |

## Downloads

Use GitHub Releases for public distribution once the repository is published.
Current local v2 release files are built in:

```text
release/
```

Recommended public assets:

- `Buckswood_Resolve_Plugins_v2_macOS.dmg`
- `Buckswood_Resolve_Plugins_v2_macOS.zip`
- `Buckswood_Resolve_Plugins_v2_Windows_Setup.exe`
- `Buckswood_Resolve_Plugins_v2_Windows.zip`
- `Buckswood_Film_Emulation_v2_Installer.dmg`
- `Buckswood_Film_Emulation_v2_Installer.pkg`
- `Buckswood_Cinematic_Tools_v2_Installer.dmg`
- `Buckswood_Cinematic_Tools_v2_Installer.pkg`
- `Buckswood_Look_DNA_v2_Installer.dmg`
- `Buckswood_Look_DNA_v2_Installer.pkg`
- `Buckswood_Nuke_OFX_v2.zip`
- `Buckswood_Premiere_Native_macOS.zip`
- `Buckswood_Premiere_Native_Windows.zip`
- `Buckswood_Premiere_Plugins_Setup.exe`
- `Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt`

## Build

macOS smoke tests:

```bash
make -C Buckswood_Fake_Diagnostic smoketest
make -C Buckswood_Lens_Physics smoketest
make -C Buckswood_AI_Photorealizer smoketest
make -C Buckswood_Film_Emulation smoketest
make -C Buckswood_Cinematic_Tools smoketest
make -C Buckswood_Cinematic_Tools descriptor-test
make -C Buckswood_Look_DNA all
make -C Buckswood_Look_DNA companion-test
```

macOS bundles:

```bash
make -C Buckswood_Fake_Diagnostic bundle
make -C Buckswood_Lens_Physics bundle
make -C Buckswood_AI_Photorealizer bundle
make -C Buckswood_Film_Emulation bundle
make -C Buckswood_Cinematic_Tools bundle
make -C Buckswood_Look_DNA bundle
```

Windows Resolve package from macOS:

```bash
bash scripts/build_windows_release.sh
```

Apple GPU performance and CPU/GPU quality benchmark:

```bash
bash scripts/run_gpu_performance_benchmark.sh
```

See [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) for backend selection,
quality tolerances, and measured results.

Nuke OFX package:

```bash
bash scripts/build_nuke_ofx_release.sh
```

Premiere native packages require the Adobe Premiere Pro C++ SDK:

```bash
bash premiere_port/native/scripts/build_premiere_plugins.sh "/path/to/Premiere Pro C++ SDK"
bash premiere_port/native/scripts/build_premiere_windows_plugins.sh "/path/to/Premiere Pro C++ SDK"
```

That script downloads nothing by itself. It expects the llvm-mingw toolchain at:

```text
third_party/toolchains/llvm-mingw-20260616-ucrt-macos-universal
```

The GitHub Actions release workflow downloads that toolchain automatically.

## Distribution Notes

Do not commit local build outputs, notarization credentials, private LUT packs, rendered videos, or the local Windows toolchain. Those are intentionally excluded by `.gitignore`.

For GitHub, publish clean source in the repository and attach installers to GitHub Releases.

## License

Original Buckswood source code is released under the MIT License. Third-party
components keep their original licenses. Lensfun-derived profile data requires
attribution; keep `Buckswood_Lens_Physics/assets/LENSFUN_ATTRIBUTION.md` with
any redistributed Lensfun-derived files.
