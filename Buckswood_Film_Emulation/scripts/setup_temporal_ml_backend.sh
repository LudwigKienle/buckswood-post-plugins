#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="$ROOT_DIR/.venv_temporal_ml"

python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/python" -m pip install --upgrade pip
"$VENV_DIR/bin/python" -m pip install numpy Pillow

echo
echo "Temporal ML helper environment is ready:"
echo "$VENV_DIR"
echo
echo "Fallback usage:"
echo "$VENV_DIR/bin/python $ROOT_DIR/scripts/temporal_ml_reconstruct.py /path/to/input_frames /path/to/output_frames --mode fallback"
echo
echo "For BasicVSR++ or RealBasicVSR, install the backend separately and use --mode basicvsrpp or --mode external."
