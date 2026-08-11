# Open-source debanding survey

Research snapshot: 2026-08-11.

This survey informed the design, but the Buckswood core is independently
implemented. Source from copyleft or unlicensed repositories is not copied,
vendored, linked, or redistributed.

| Project | Approach | License observed | Decision |
| --- | --- | --- | --- |
| [deepDeband](https://github.com/RaymondLZhou/deepDeband) | Paired patch dataset, pix2pix-style network, weighted patch fusion | MIT | Candidate for a separately validated optional backend |
| [GGDBN](https://github.com/SCUT-SuFM/GGDBN) | Full-resolution detail and gradient branches with guided fusion | MIT | Gradient guidance inspired the native detector/reconstruction split |
| [INDI Debanding](https://github.com/ksasso1028/indi-debanding) | Iterative generative restoration in frequency space | MIT | Interesting offline/companion path; too heavy and not video-finished for V1 |
| [BitNet](https://github.com/kamkyu94/BitNet) | Learned bit-depth expansion | MIT | Useful reference for quantization-aware training |
| [FBCNN](https://github.com/jiaxi-jiang/FBCNN) | Learned JPEG artifact removal | Apache-2.0 | Adjacent pre-cleanup research, not a debanding engine |
| [vs-deband](https://github.com/Jaded-Encoding-Thaumaturgy/vs-deband) | VapourSynth debanding helpers/wrappers | MIT | Workflow reference only |
| [libplacebo](https://github.com/haasn/libplacebo) | Tunable GPU debanding and dithering | LGPL-2.1-or-later | Algorithmic reference only; not linked or copied |
| [flash3kyuu_deband](https://github.com/SAPikachu/flash3kyuu_deband) | Range-limited randomized sampling and grain | GPL-3.0 | Algorithmic reference only; incompatible with MIT redistribution |
| [neo_f3kdb](https://github.com/HomeOfAviSynthPlusEvolution/neo_f3kdb) | SIMD continuation of f3kdb | GPL-3.0 | Performance reference only; no source reused |
| [Debander_OpenFX](https://github.com/wrzwicky/Debander_OpenFX) | Early OpenFX debander | No license detected | Not reusable; README also reports ineffective Resolve behavior |
| [WaveMamba Debanding](https://github.com/xinyiW915/Debanding-PCS2025) | Frequency-aware Mamba architecture | No license detected | Paper-level reference only; no code or weights redistributed |
| [BDINN](https://github.com/csxyhe/BDINN) | Cross-scale invertible networks and banded deformable convolution | No license detected | Paper-level reference only |

## V1 design choices

1. **Gradient-guided, not globally blurred.** A weak-contour confidence mask is
   computed separately from the reconstruction candidate.
2. **Three spatial scales.** Broad bands need farther samples while fine
   contours need short-radius evidence.
3. **Structure guards.** Local edge and texture measurements suppress repair on
   text, silhouettes, grain, skin, fabric, and intentional graphic boundaries.
4. **Luma-first reconstruction.** Chroma is repaired independently so users can
   avoid color bleeding.
5. **Targeted dither.** High-frequency, zero-mean dither is restricted to
   detected gradient regions rather than covering the whole image.
6. **Frame-order independence.** Static and frame-indexed modes are both pure
   functions of coordinates, parameters, and OFX time. No previous-frame cache
   is used, matching Baselight's host constraints.

## Neural backend gate

A future neural backend must pass all of these checks before distribution:

- explicit license for code and the exact checkpoint
- documented training-data rights and intended use
- full-float color pipeline with HDR/negative-value tests
- tiled or overlap-safe inference without patch seams
- deterministic out-of-order rendering in Baselight
- temporal flicker tests on gradients, cuts, and camera noise
- CPU fallback and GPU memory budgeting
- no network access required during host rendering
