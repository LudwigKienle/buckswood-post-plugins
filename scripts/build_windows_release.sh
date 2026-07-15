#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN_DIR="$ROOT_DIR/third_party/toolchains/llvm-mingw-20260616-ucrt-macos-universal"
CXX="$TOOLCHAIN_DIR/bin/clang++"
WINDRES="$TOOLCHAIN_DIR/bin/llvm-windres"
OBJDUMP="$TOOLCHAIN_DIR/bin/llvm-objdump"
WINDOWS_DIR="$ROOT_DIR/windows_release"
BUILD_DIR="$WINDOWS_DIR/build"
PAYLOAD_DIR="$WINDOWS_DIR/Buckswood_Resolve_Plugins_Windows"
INSTALLER_DIR="$WINDOWS_DIR/installer"
PUBLIC_DIR="$WINDOWS_DIR/public"
SETUP_EXE="$PUBLIC_DIR/Buckswood_Resolve_Plugins_Setup.exe"
ZIP_PATH="$PUBLIC_DIR/Buckswood_Resolve_Plugins_Windows.zip"

[[ -x "$CXX" ]] || { echo "Missing toolchain: $CXX" >&2; exit 1; }

rm -rf "$BUILD_DIR" "$PAYLOAD_DIR" "$PUBLIC_DIR"
mkdir -p "$BUILD_DIR" "$PUBLIC_DIR"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodAIPhotorealizer.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodCinematicTools.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLookDNA.ofx.bundle/Contents/Win64"
mkdir -p "$PAYLOAD_DIR/ResolveLUT"
mkdir -p "$PAYLOAD_DIR/ML_Companion/scripts" "$PAYLOAD_DIR/ML_Companion/models/hdrtvnet_plus"

COMMON_FLAGS=(
  --target=x86_64-w64-windows-gnu
  -std=c++17
  -O3
  -DWIN32
  -D_WINDOWS
  -static
  -static-libgcc
  -static-libstdc++
)

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_Fake_Diagnostic/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_Fake_Diagnostic/src/FakeDiagnosticCore.cpp" \
  "$ROOT_DIR/Buckswood_Fake_Diagnostic/src/BuckswoodFakeDiagnosticOFX.cpp" \
  -o "$BUILD_DIR/BuckswoodFakeDiagnostic.ofx"

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_Lens_Physics/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp" \
  "$ROOT_DIR/Buckswood_Lens_Physics/src/BuckswoodLensPhysicsOFX.cpp" \
  -o "$BUILD_DIR/BuckswoodLensPhysics.ofx"

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_AI_Photorealizer/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp" \
  "$ROOT_DIR/Buckswood_AI_Photorealizer/src/BuckswoodAIPhotorealizerOFX.cpp" \
  -o "$BUILD_DIR/BuckswoodAIPhotorealizer.ofx"

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_Film_Emulation/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_Film_Emulation/src/FilmEmulationCore.cpp" \
  "$ROOT_DIR/Buckswood_Film_Emulation/src/BuckswoodFilmEmulationOFX.cpp" \
  -o "$BUILD_DIR/BuckswoodFilmEmulation.ofx"

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_Cinematic_Tools/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_Cinematic_Tools/src/CinematicToolsCore.cpp" \
  "$ROOT_DIR/Buckswood_Cinematic_Tools/src/BuckswoodCinematicToolsOFX.cpp" \
  -o "$BUILD_DIR/BuckswoodCinematicTools.ofx"

"$CXX" "${COMMON_FLAGS[@]}" \
  -I"$ROOT_DIR/Buckswood_Look_DNA/include" \
  -I"$ROOT_DIR/third_party/openfx/include" \
  -shared \
  "$ROOT_DIR/Buckswood_Look_DNA/src/LookDNACore.cpp" \
  "$ROOT_DIR/Buckswood_Look_DNA/src/ReferenceImageLoader.cpp" \
  "$ROOT_DIR/Buckswood_Look_DNA/src/BuckswoodLookDNAOFX.cpp" \
  -lwindowscodecs -lole32 -luuid \
  -o "$BUILD_DIR/BuckswoodLookDNA.ofx"

cp "$BUILD_DIR/BuckswoodFakeDiagnostic.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle/Contents/Win64/BuckswoodFakeDiagnostic.ofx"
cp "$BUILD_DIR/BuckswoodLensPhysics.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle/Contents/Win64/BuckswoodLensPhysics.ofx"
cp "$BUILD_DIR/BuckswoodAIPhotorealizer.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodAIPhotorealizer.ofx.bundle/Contents/Win64/BuckswoodAIPhotorealizer.ofx"
cp "$BUILD_DIR/BuckswoodFilmEmulation.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle/Contents/Win64/BuckswoodFilmEmulation.ofx"
cp "$BUILD_DIR/BuckswoodCinematicTools.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodCinematicTools.ofx.bundle/Contents/Win64/BuckswoodCinematicTools.ofx"
cp "$BUILD_DIR/BuckswoodLookDNA.ofx" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLookDNA.ofx.bundle/Contents/Win64/BuckswoodLookDNA.ofx"
cp "$ROOT_DIR/Buckswood_Fake_Diagnostic/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_Lens_Physics/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_AI_Photorealizer/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodAIPhotorealizer.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_Film_Emulation/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodCinematicTools.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_Look_DNA/Info.plist" "$PAYLOAD_DIR/OFX/Plugins/BuckswoodLookDNA.ofx.bundle/Contents/Info.plist"
cp "$ROOT_DIR/Buckswood_Lens_Physics/dctl/Buckswood_Lens_Physics_v01.dctl" "$PAYLOAD_DIR/ResolveLUT/"
cp "$ROOT_DIR/Buckswood_AI_Photorealizer/dctl/Buckswood_AI_Photorealizer_v01.dctl" "$PAYLOAD_DIR/ResolveLUT/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/scripts/radiance_cache.py" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/scripts/hdrtvnet_plus_adapter.py" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/scripts/setup_ml_backend.sh" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/scripts/setup_ml_backend_windows.bat" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Film_Emulation/scripts/temporal_ml_reconstruct.py" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Film_Emulation/scripts/setup_temporal_ml_backend.sh" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/requirements-companion.txt" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Film_Emulation/requirements-temporal-ml.txt" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Film_Emulation/TEMPORAL_ML_BACKEND.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Film_Emulation/RESEARCH_NOTES.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/models/hdrtvnet_plus/AGCM.pth" "$PAYLOAD_DIR/ML_Companion/models/hdrtvnet_plus/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/models/hdrtvnet_plus/ATTRIBUTION.md" "$PAYLOAD_DIR/ML_Companion/models/hdrtvnet_plus/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/models/hdrtvnet_plus/LICENSE_HDRTVNet-plus" "$PAYLOAD_DIR/ML_Companion/models/hdrtvnet_plus/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/DOCUMENTATION_DE.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/DOCUMENTATION_EN.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Cinematic_Tools/OPEN_SOURCE_ROADMAP.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$ROOT_DIR/Buckswood_Look_DNA/scripts/analyze_reference.py" "$PAYLOAD_DIR/ML_Companion/scripts/"
cp "$ROOT_DIR/Buckswood_Look_DNA/requirements-companion.txt" "$PAYLOAD_DIR/ML_Companion/requirements-look-dna.txt"
cp "$ROOT_DIR/Buckswood_Look_DNA/DOCUMENTATION_DE.md" "$PAYLOAD_DIR/ML_Companion/LOOK_DNA_DOCUMENTATION_DE.md"
cp "$ROOT_DIR/Buckswood_Look_DNA/DOCUMENTATION_EN.md" "$PAYLOAD_DIR/ML_Companion/LOOK_DNA_DOCUMENTATION_EN.md"
cp "$ROOT_DIR/Buckswood_Look_DNA/LOOK_PROFILE_FORMAT.md" "$PAYLOAD_DIR/ML_Companion/"
cp "$WINDOWS_DIR/README_WINDOWS.txt" "$PUBLIC_DIR/README_WINDOWS.txt"

python3 - "$INSTALLER_DIR/payload_data.h" \
  fake_info_plist "$ROOT_DIR/Buckswood_Fake_Diagnostic/Info.plist" \
  fake_ofx "$BUILD_DIR/BuckswoodFakeDiagnostic.ofx" \
  lens_info_plist "$ROOT_DIR/Buckswood_Lens_Physics/Info.plist" \
  lens_ofx "$BUILD_DIR/BuckswoodLensPhysics.ofx" \
  photo_info_plist "$ROOT_DIR/Buckswood_AI_Photorealizer/Info.plist" \
  photo_ofx "$BUILD_DIR/BuckswoodAIPhotorealizer.ofx" \
  film_info_plist "$ROOT_DIR/Buckswood_Film_Emulation/Info.plist" \
  film_ofx "$BUILD_DIR/BuckswoodFilmEmulation.ofx" \
  cinematic_info_plist "$ROOT_DIR/Buckswood_Cinematic_Tools/Info.plist" \
  cinematic_ofx "$BUILD_DIR/BuckswoodCinematicTools.ofx" \
  look_info_plist "$ROOT_DIR/Buckswood_Look_DNA/Info.plist" \
  look_ofx "$BUILD_DIR/BuckswoodLookDNA.ofx" \
  lens_dctl "$ROOT_DIR/Buckswood_Lens_Physics/dctl/Buckswood_Lens_Physics_v01.dctl" \
  photo_dctl "$ROOT_DIR/Buckswood_AI_Photorealizer/dctl/Buckswood_AI_Photorealizer_v01.dctl" <<'PY'
import pathlib
import sys

out = pathlib.Path(sys.argv[1])
pairs = list(zip(sys.argv[2::2], sys.argv[3::2]))
with out.open("w", encoding="utf-8") as fh:
    fh.write("#pragma once\n#include <cstddef>\n\nnamespace payload {\n\n")
    for name, file_name in pairs:
        data = pathlib.Path(file_name).read_bytes()
        fh.write(f"const unsigned char {name}[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            fh.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        fh.write("};\n")
        fh.write(f"const std::size_t {name}_size = sizeof({name});\n\n")
    fh.write("} // namespace payload\n")
PY

(
  cd "$INSTALLER_DIR"
  "$WINDRES" --target=pe-x86-64 installer.rc "$BUILD_DIR/installer.res"
)

"$CXX" --target=x86_64-w64-windows-gnu -std=c++17 -O2 -static -static-libgcc -static-libstdc++ \
  -mwindows -municode \
  -I"$INSTALLER_DIR" \
  "$INSTALLER_DIR/installer.cpp" \
  "$BUILD_DIR/installer.res" \
  -lshell32 -lole32 -luuid \
  -o "$SETUP_EXE"

"$OBJDUMP" -p "$SETUP_EXE" | grep -E "Subsystem|DLL Name" || true

(
  cd "$WINDOWS_DIR"
  rm -f "$ZIP_PATH"
  zip -r "$ZIP_PATH" Buckswood_Resolve_Plugins_Windows README_WINDOWS.txt -x "*/.DS_Store" "__MACOSX/*" >/dev/null
)

(
  cd "$PUBLIC_DIR"
  shasum -a 256 Buckswood_Resolve_Plugins_Setup.exe Buckswood_Resolve_Plugins_Windows.zip README_WINDOWS.txt > SHA256SUMS.txt
)

echo
echo "Built Windows release:"
echo "$SETUP_EXE"
echo "$ZIP_PATH"
