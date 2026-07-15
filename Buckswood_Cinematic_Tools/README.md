# Buckswood Cinematic Tools v2

Three native OpenFX tools for AI footage in DaVinci Resolve:

- **Buckswood Frame Director v2.0**
  - intelligent aspect-ratio preview and composition guides
  - 2.39:1, 2.00:1, 1.85:1, 16:9, 4:3, 1:1 and 9:16
  - five-frame saliency analysis, shot-cut protection and Subject Lock

- **Buckswood Radiance Recover v2.0**
  - conservative inverse tone expansion for SDR and AI footage
  - highlight headroom above 1.0 in floating-point hosts
  - optional HDRTVNet++ ML cache with per-pixel confidence

- **Buckswood Temporal Integrity v2.0**
  - detects frame-to-frame flicker and unstable texture
  - five-frame consensus, Motion Guard, Edge Protection and Ghost Guard
  - error, confidence, motion and ghost-risk views

## Build and test

```bash
make clean
make all
```

Companion cache tests:

```bash
python3 -m venv .ml-venv
.ml-venv/bin/pip install -r requirements-companion.txt
make companion-test PYTHON=.ml-venv/bin/python
```

## Install for the current user

```bash
make install-user
```

Restart Resolve and open `Color > Effects > Buckswood`.

## ML workflow

Export a numbered PNG or TIFF image sequence from Resolve. Then:

```bash
./scripts/setup_ml_backend.sh

.ml-venv/bin/python scripts/hdrtvnet_plus_adapter.py \
  --input-dir /path/to/source-frames \
  --output-dir /path/to/model-output

.ml-venv/bin/python scripts/radiance_cache.py build \
  --source-dir /path/to/source-frames \
  --model-output-dir /path/to/model-output \
  --output-dir /path/to/buckswood-cache \
  --start-frame 0
```

In Radiance Recover:

1. Enable `Use ML Cache`.
2. Select the generated directory in `ML Cache Directory`.
3. Set `Cache Frame Offset` if the clip does not begin on timeline frame zero.
4. Check `ML Cache Confidence`.
5. Return to `Recovered Image`.

The OFX always falls back to its native reconstruction when a cache frame is
missing or invalid.

## Important

Radiance Recover reconstructs plausible radiance. It cannot prove what the
original sensor captured in fully clipped regions. ML pixels are blended only
where the cache confidence exceeds the selected threshold.
