#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

make install-user
codesign --force --deep --sign - "$HOME/Library/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle" >/dev/null 2>&1 || true
rm -f "$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCache.xml" \
      "$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCacheV2.xml" 2>/dev/null || true
echo "Restart DaVinci Resolve so it rebuilds the OFX cache."
echo "Plugin installed at:"
echo "$HOME/Library/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle"
