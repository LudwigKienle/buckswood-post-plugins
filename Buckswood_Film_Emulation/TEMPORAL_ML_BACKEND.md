# Temporal ML Backend Notes

`Buckswood Film Emulation v2.0` has two temporal paths:

1. Native OFX temporal reconstruction.
2. Optional companion preprocessing with `scripts/temporal_ml_reconstruct.py`.

The OFX path is fast and host-native. It samples neighbor frames and blends them with a motion guard. Use it directly in Resolve when the clip has light AI shimmer or unstable micro-texture.

The companion path is for heavier repair. Run it on an image sequence before bringing the clip into Resolve, or export a sequence from Resolve, process it, then reimport the processed sequence.

## Setup

```bash
bash Buckswood_Film_Emulation/scripts/setup_temporal_ml_backend.sh
```

This creates:

```text
Buckswood_Film_Emulation/.venv_temporal_ml
```

with `numpy` and `Pillow` for the fallback backend.

## Fallback Backend

The fallback backend does motion-guarded temporal blending. It is not a neural model, but it is useful for a quick offline pass.

```bash
Buckswood_Film_Emulation/.venv_temporal_ml/bin/python \
  Buckswood_Film_Emulation/scripts/temporal_ml_reconstruct.py \
  /path/to/input_frames \
  /path/to/output_frames \
  --mode fallback \
  --strength 0.30 \
  --stability 0.70 \
  --motion-guard 0.85
```

Input frames should be sorted still images such as:

```text
shot_0001.png
shot_0002.png
shot_0003.png
```

## BasicVSR++ Adapter

If BasicVSR++ is installed separately, the adapter can call its official demo entrypoint:

```bash
Buckswood_Film_Emulation/.venv_temporal_ml/bin/python \
  Buckswood_Film_Emulation/scripts/temporal_ml_reconstruct.py \
  /path/to/input_frames \
  /path/to/output_frames \
  --mode basicvsrpp \
  --basicvsrpp-dir /path/to/BasicVSR_PlusPlus \
  --config /path/to/config.py \
  --checkpoint /path/to/checkpoint.pth
```

The script does not bundle weights. Download and license-check model weights separately.

## External Backend

You can also wrap another command:

```bash
Buckswood_Film_Emulation/.venv_temporal_ml/bin/python \
  Buckswood_Film_Emulation/scripts/temporal_ml_reconstruct.py \
  /path/to/input_frames \
  /path/to/output_frames \
  --mode external \
  --external-command "python my_restore.py --input {input_dir} --output {output_dir}"
```

Placeholders:

- `{input_dir}` becomes the input frame directory.
- `{output_dir}` becomes the output frame directory.

## Recommended Workflow

For AI footage:

1. Export image sequence from Resolve.
2. Run the companion backend.
3. Reimport the processed sequence.
4. Apply Buckswood Film Emulation v2.0 with `Temporal AI Reconstruction` at 0.00 or very low.

For lighter shimmer:

1. Stay inside Resolve.
2. Set `Temporal AI Reconstruction` to 0.15-0.35.
3. Keep `Temporal Motion Guard` around 0.75-0.90.
