# Open-Source ML Roadmap

V2 ships the MIT-licensed HDRTVNet++ AGCM checkpoint as an optional local
companion model. The model never runs inside Resolve. The OFX reads only a
validated half-float cache and falls back to its deterministic path when needed.

## Radiance Recover

- HDRTVNet++: https://github.com/xiaom233/HDRTVNet-plus
  - MIT
  - SDR-to-HDRTV global mapping, local enhancement and highlight refinement
- VITM-TC: https://github.com/ye3why/VITM-TC
  - MIT
  - video inverse tone mapping using temporal clues
- HDRTVDM: https://github.com/AndreGuo/HDRTVDM
  - MPL-2.0
  - practical SDR-to-HDRTV degradation modeling

Implemented V2 architecture:

1. Resolve exports a numbered source sequence.
2. The local companion runs HDRTVNet++ AGCM on MPS, CUDA or CPU.
3. A five-frame temporal pass creates recovery confidence.
4. Results are cached as half-float RGB plus half-float confidence.
5. OFX validates dimensions, version and frame number before reading.
6. Missing or stale cache data falls back to the deterministic V2 path.

## Frame Director

- MediaPipe: https://github.com/google-ai-edge/mediapipe
  - Apache-2.0
  - face, pose and object landmarks
- SAM 2: https://github.com/facebookresearch/sam2
  - Apache-2.0
  - video object segmentation and tracking
- CADB: https://github.com/bcmi/Image-Composition-Assessment-Dataset-CADB
  - MIT
  - composition assessment research and dataset tooling
- NIMA implementation: https://github.com/idealo/image-quality-assessment
  - Apache-2.0
  - aesthetic and technical image scoring

Future model-backed framing:

1. Detect shot boundaries.
2. Track protected subjects, faces and gaze direction.
3. Score multiple crop candidates per shot.
4. Solve a smooth crop path with acceleration limits.
5. Let the user pin a subject or override the path.

## Licensing note

An open-source project still has to obey dependency and model-weight licenses.
Research repositories with a non-commercial-only license should remain
optional and must not be bundled into a generally redistributable binary
without the required permission.
