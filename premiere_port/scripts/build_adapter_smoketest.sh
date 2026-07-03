#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT_DIR="$ROOT_DIR/premiere_port"
BUILD_DIR="$PORT_DIR/build"

mkdir -p "$BUILD_DIR"

clang++ -std=c++17 -O2 \
    -I"$PORT_DIR/src" \
    -I"$ROOT_DIR/Buckswood_AI_Photorealizer/include" \
    -I"$ROOT_DIR/Buckswood_Lens_Physics/include" \
    "$PORT_DIR/src/BuckswoodPremiereAdapter.cpp" \
    "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp" \
    "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp" \
    "$PORT_DIR/src/PremiereAdapterSmokeTest.cpp" \
    -o "$BUILD_DIR/premiere_adapter_smoketest"

"$BUILD_DIR/premiere_adapter_smoketest"
