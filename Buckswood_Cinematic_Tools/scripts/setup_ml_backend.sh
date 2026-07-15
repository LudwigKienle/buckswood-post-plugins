#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="${BUCKSWOOD_ML_VENV:-$ROOT_DIR/.ml-venv}"
PYTHON="${PYTHON:-python3}"

"$PYTHON" -m venv "$VENV_DIR"
"$VENV_DIR/bin/python" -m pip install --upgrade pip wheel
"$VENV_DIR/bin/python" -m pip install \
    "numpy>=1.26,<3" \
    "Pillow>=11,<13" \
    "torch>=2.2,<3"

echo
echo "Buckswood ML companion is ready:"
echo "$VENV_DIR/bin/python"
echo
echo "Run HDRTVNet++:"
echo "\"$VENV_DIR/bin/python\" \"$ROOT_DIR/scripts/hdrtvnet_plus_adapter.py\" --input-dir /path/to/frames --output-dir /path/to/model-output"
