#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT_DIR="$ROOT_DIR/premiere_port"
RELEASE_DIR="$PORT_DIR/release"
ZIP_PATH="$RELEASE_DIR/Buckswood_Premiere_Port_Source.zip"

mkdir -p "$RELEASE_DIR"
rm -f "$ZIP_PATH" "$RELEASE_DIR/SHA256SUMS.txt"

(
    cd "$ROOT_DIR"
    zip -r "$ZIP_PATH" \
        premiere_port/CMakeLists.txt \
        premiere_port/README_PREMIERE_PORT.md \
        premiere_port/README_PREMIERE_NATIVE.md \
        premiere_port/src \
        premiere_port/scripts \
        premiere_port/native \
        Buckswood_AI_Photorealizer/include \
        Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp \
        Buckswood_Lens_Physics/include \
        Buckswood_Lens_Physics/src/LensPhysicsCore.cpp \
        >/dev/null
)

(
    cd "$RELEASE_DIR"
    shasum -a 256 "$(basename "$ZIP_PATH")" > SHA256SUMS.txt
)

echo "$ZIP_PATH"
