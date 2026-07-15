# Buckswood Film Emulation v2.0 Research Notes

These notes document the design references used for V2. They are references, not copied implementations.

## Dehancer Reference

Dehancer's public learning material frames film emulation as a chain of tools: Input, Color Spaces, Film Profiles, Film Developer, Film Compression, Expand, Print, CMY Color Head, Film Grain, Halation, Bloom, Film Breath, Gate Weave, Film Damage, Vignette, LUT generation, and output handling.

What V2 borrowed conceptually:

- Film emulation should be staged, not just LUT-based.
- Grain should respond to image tone and format, not behave like a flat overlay.
- Halation and bloom should be controllable separately but designed to work together.
- Film compression is a critical part of making digital highlights less harsh.
- CMY printer-light-style controls are more useful than generic RGB color shifts for final trims.

V2 difference:

- Buckswood has fewer stock identities and focuses on AI-footage correction.
- Temporal reconstruction and AI Rec.709 presets are first-class controls.
- Process modes allow `Color Only` and `Texture Only` outputs for VFX/compositing workflows.

## Cullen Kelly Genesis Reference

Genesis is positioned as process modeling rather than simply imitating a visual result. Public Genesis material describes a path from digital sensor light through film negative exposure, lab development, and print, with controls like Push/Pull, Interlayer, Printer Points, Bleach Bypass, Neutralize Curves, Grain/Halation, Texture-only Mode, LUT Export, and HDR output.

What V2 learned:

- A serious film tool needs process order and predictable controls.
- Texture-only output is valuable because color and texture are different creative decisions.
- Printer-style controls make a film system feel more photographically directed.
- Neutralizing curves matters when users want film texture without the full contrast identity.

V2 difference:

- Genesis is a premium measured film system; Buckswood V2 is an open practical tool.
- Buckswood V2 is deliberately AI-footage-first: Rec.709 AI input, deplastic stocks, temporal shimmer controls, and skin protection.
- The plugin exposes diagnostic view modes so users can see what the algorithm is doing.

## Open-Source Research References

`spektrafilm` is a useful conceptual reference for spectral film process thinking, halation/scattering parameters, print steps, and stochastic emulsion behavior. It is GPL-3.0, so no code was copied into this project.

`BasicVSR++` and `RealBasicVSR` are useful references for temporal propagation, alignment, and video restoration. Their public repositories are Apache-2.0, making them plausible optional external backends. V2 ships only a lightweight companion adapter and fallback method, not bundled model weights.

## Practical Design Rule

The plugin should not try to beat Dehancer or Genesis by pretending to have more measured stocks. It should win by doing something they are not centered on:

```text
AI footage -> repair plasticity, stabilize temporal artifacts, add believable film process, keep the workflow fast.
```

That is the Buckswood lane.

## Source Links

- Dehancer Learn: https://www.dehancer.com/learn
- Dehancer Film Grain: https://www.dehancer.com/learn/article/grain
- Dehancer Halation: https://www.dehancer.com/learn/article/halation
- Cullen Kelly Genesis: https://cullenkellycolor.com/genesis
- spektrafilm: https://github.com/andreavolpato/spektrafilm
- BasicVSR++: https://github.com/ckkelvinchan/BasicVSR_PlusPlus
- RealBasicVSR: https://github.com/ckkelvinchan/realbasicvsr
