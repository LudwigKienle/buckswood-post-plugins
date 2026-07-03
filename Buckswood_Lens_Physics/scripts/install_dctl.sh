#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT_DIR/dctl/Buckswood_Lens_Physics_v01.dctl"
DST="/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/Buckswood_Lens_Physics_v01.dctl"

mkdir -p "$(dirname "$DST")"
cp "$SRC" "$DST"
echo "Installed DCTL to: $DST"
echo "Refresh LUTs in Resolve or restart Resolve."

