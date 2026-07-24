#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/performance"
CXX="${CXX:-clang++}"
ARCH="${ARCH:-$(uname -m)}"

mkdir -p "$BUILD_DIR"

"$CXX" \
  -std=c++17 \
  -O3 \
  -arch "$ARCH" \
  -I"$ROOT_DIR/shared" \
  -I"$ROOT_DIR/Buckswood_AI_Photorealizer/include" \
  -I"$ROOT_DIR/Buckswood_Lens_Physics/include" \
  -framework Metal \
  -framework Foundation \
  "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp" \
  "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerMetal.mm" \
  "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp" \
  "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsMetal.mm" \
  "$ROOT_DIR/tests/gpu_performance_benchmark.mm" \
  -o "$BUILD_DIR/gpu_performance_benchmark"

"$BUILD_DIR/gpu_performance_benchmark"
