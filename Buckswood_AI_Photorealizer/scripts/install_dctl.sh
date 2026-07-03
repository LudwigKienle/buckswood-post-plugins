#!/usr/bin/env bash
set -euo pipefail

src="$(cd "$(dirname "$0")/.." && pwd)/dctl/Buckswood_AI_Photorealizer_v01.dctl"
dst="/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/Buckswood_AI_Photorealizer_v01.dctl"

cp "$src" "$dst"
echo "Installed $dst"
echo "In Resolve: Refresh LUTs or restart, then load Buckswood_AI_Photorealizer_v01.dctl."
