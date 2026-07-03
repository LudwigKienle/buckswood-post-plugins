#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/output"
RESOLVE_LUT_DIR="/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT"

if ! compgen -G "$OUT_DIR/*.cube" > /dev/null; then
  echo "No .cube files found in $OUT_DIR"
  exit 1
fi

mkdir -p "$RESOLVE_LUT_DIR"
cp "$OUT_DIR"/*.cube "$RESOLVE_LUT_DIR/"
echo "Installed LUTs to: $RESOLVE_LUT_DIR"
echo "Refresh LUTs in DaVinci Resolve or restart Resolve."

