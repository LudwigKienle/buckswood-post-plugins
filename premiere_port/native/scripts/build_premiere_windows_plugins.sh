#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NATIVE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT_DIR="$(cd "$NATIVE_DIR/.." && pwd)"
ROOT_DIR="$(cd "$PORT_DIR/.." && pwd)"

DEFAULT_SDK="/Users/ludwig.kienle/Downloads/Premiere Pro 26.0 C++ SDK 2"
PREMIERE_SDK_PATH="${1:-${PREMIERE_SDK_PATH:-$DEFAULT_SDK}}"
if [[ ! -d "$PREMIERE_SDK_PATH/Examples/Headers" ]]; then
    echo "Premiere SDK headers not found: $PREMIERE_SDK_PATH" >&2
    exit 1
fi

TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-$ROOT_DIR/third_party/toolchains/llvm-mingw-20260616-ucrt-macos-universal}"
CXX="$TOOLCHAIN_DIR/bin/clang++"
RC="$TOOLCHAIN_DIR/bin/llvm-rc"
WINDRES="$TOOLCHAIN_DIR/bin/llvm-windres"
READOBJ="$TOOLCHAIN_DIR/bin/llvm-readobj"
OBJDUMP="$TOOLCHAIN_DIR/bin/llvm-objdump"
NM="$TOOLCHAIN_DIR/bin/llvm-nm"

for tool in "$CXX" "$RC" "$WINDRES" "$READOBJ" "$OBJDUMP" "$NM"; do
    [[ -x "$tool" ]] || { echo "Missing tool: $tool" >&2; exit 1; }
done

MACOS_SDK="$(xcrun --sdk macosx --show-sdk-path)"
REZ="$(xcrun -find Rez)"
HOST_CLANG="$(xcrun -find clang)"

BUILD_DIR="$PORT_DIR/build/native_premiere_windows"
DIST_DIR="$PORT_DIR/windows_dist"
INSTALLER_DIR="$PORT_DIR/windows_installer"
PACKAGE_DIR="$PORT_DIR/release/Buckswood_Premiere_Native_Windows"
SETUP_EXE="$PORT_DIR/release/Buckswood_Premiere_Plugins_Setup.exe"
rm -rf "$BUILD_DIR" "$DIST_DIR" "$PACKAGE_DIR"
mkdir -p "$BUILD_DIR" "$DIST_DIR" "$PACKAGE_DIR/dist" "$PACKAGE_DIR/native/scripts" "$PORT_DIR/release"

COMMON_SOURCES=(
    "$NATIVE_DIR/src/BuckswoodPremiereNative.cpp"
    "$PORT_DIR/src/BuckswoodPremiereAdapter.cpp"
    "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp"
    "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp"
    "$ROOT_DIR/Buckswood_Fake_Diagnostic/src/FakeDiagnosticCore.cpp"
    "$ROOT_DIR/Buckswood_Film_Emulation/src/FilmEmulationCore.cpp"
    "$ROOT_DIR/Buckswood_Cinematic_Tools/src/CinematicToolsCore.cpp"
    "$ROOT_DIR/Buckswood_Look_DNA/src/LookDNACore.cpp"
)

COMMON_FLAGS=(
    --target=x86_64-w64-windows-gnu
    -std=c++17
    -O2
    -DPRWIN_ENV=1
    -DWIN_ENV=1
    -DWIN32
    -D_WINDOWS
    -fno-exceptions
    -fno-rtti
    -static
    -static-libgcc
    -static-libstdc++
    -I"$PREMIERE_SDK_PATH/Examples/Headers"
    -I"$PREMIERE_SDK_PATH/Examples/Headers/SP"
    -I"$ROOT_DIR/Buckswood_AI_Photorealizer/include"
    -I"$ROOT_DIR/Buckswood_Lens_Physics/include"
    -I"$ROOT_DIR/Buckswood_Fake_Diagnostic/include"
    -I"$ROOT_DIR/Buckswood_Film_Emulation/include"
    -I"$ROOT_DIR/Buckswood_Cinematic_Tools/include"
    -I"$ROOT_DIR/Buckswood_Look_DNA/include"
    -I"$PORT_DIR/src"
)

extract_pipl_payload() {
    local name="$1"
    local resource="$2"
    local rsrc="$BUILD_DIR/$name.rsrc"
    local payload="$BUILD_DIR/$name.pipl.bin"
    local preprocessed_resource="$BUILD_DIR/$name.preprocessed.r"

    "$HOST_CLANG" \
        -E \
        -x c \
        -P \
        -DSystemSevenOrLater=1 \
        -I"$NATIVE_DIR/resources" \
        -I"$PREMIERE_SDK_PATH/Examples/Headers" \
        -I"$PREMIERE_SDK_PATH/Examples/Headers/SP" \
        -isysroot "$MACOS_SDK" \
        "$resource" \
        -o "$preprocessed_resource"

    "$REZ" \
        -o "$rsrc" \
        -useDF \
        -script Roman \
        -arch x86_64 \
        -isysroot "$MACOS_SDK" \
        "$preprocessed_resource"

    python3 - "$rsrc" "$payload" <<'PY'
from pathlib import Path
import struct
import sys

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
data = src.read_bytes()
if len(data) < 20:
    raise SystemExit(f"Resource fork is too small: {src}")

data_offset, _map_offset, data_length, _map_length = struct.unpack(">IIII", data[:16])
if data_offset + 4 > len(data):
    raise SystemExit(f"Invalid resource data offset in {src}")

payload_length = struct.unpack(">I", data[data_offset:data_offset + 4])[0]
payload_start = data_offset + 4
payload_end = payload_start + payload_length
if payload_length <= 0 or payload_end > len(data) or payload_length > data_length:
    raise SystemExit(f"Invalid PiPL payload length in {src}")

payload = data[payload_start:payload_end]
if b"8BIM" not in payload[:32]:
    raise SystemExit(f"Extracted data does not look like a PiPL payload: {src}")

dst.write_bytes(payload)
print(f"Extracted {payload_length} bytes to {dst}")
PY
}

write_windows_resource() {
    local name="$1"
    local payload_basename="$name.pipl.bin"
    local rc_file="$BUILD_DIR/$name.rc"
    local res_file="$BUILD_DIR/$name.res"

    cat > "$rc_file" <<RC
16000 PiPL "$payload_basename"
RC
    (
        cd "$BUILD_DIR"
        "$RC" /fo "$(basename "$res_file")" "$(basename "$rc_file")"
    )
}

write_export_def() {
    local def_file="$BUILD_DIR/BuckswoodPremiere.def"
    cat > "$def_file" <<'DEF'
EXPORTS
    xFilter
DEF
}

build_effect() {
    local name="$1"
    local define="$2"
    local resource="$3"

    extract_pipl_payload "$name" "$resource"
    write_windows_resource "$name"

    "$CXX" "${COMMON_FLAGS[@]}" \
        "$define" \
        -shared \
        "${COMMON_SOURCES[@]}" \
        "$BUILD_DIR/$name.res" \
        "$BUILD_DIR/BuckswoodPremiere.def" \
        -Wl,--exclude-all-symbols \
        -Wl,--no-insert-timestamp \
        -o "$DIST_DIR/$name.prm"

    "$NM" -g "$DIST_DIR/$name.prm" | grep -q " T xFilter"
    "$READOBJ" --coff-resources "$DIST_DIR/$name.prm" | grep -q "Type: PIPL"
    "$OBJDUMP" -p "$DIST_DIR/$name.prm" | grep -q "DLL name: $name.prm"
    echo "Built $DIST_DIR/$name.prm"
}

write_export_def

build_effect \
    "BuckswoodAIPhotorealizer" \
    "-DBUCKSWOOD_PREMIERE_PHOTOREALIZER=1" \
    "$NATIVE_DIR/resources/BuckswoodAIPhotorealizerPiPL.r"

build_effect \
    "BuckswoodLensPhysics" \
    "-DBUCKSWOOD_PREMIERE_LENS=1" \
    "$NATIVE_DIR/resources/BuckswoodLensPhysicsPiPL.r"

build_effect \
    "BuckswoodFakeDiagnostic" \
    "-DBUCKSWOOD_PREMIERE_FAKE_DIAGNOSTIC=1" \
    "$NATIVE_DIR/resources/BuckswoodFakeDiagnosticPiPL.r"

build_effect \
    "BuckswoodFilmEmulation" \
    "-DBUCKSWOOD_PREMIERE_FILM_EMULATION=1" \
    "$NATIVE_DIR/resources/BuckswoodFilmEmulationPiPL.r"

build_effect \
    "BuckswoodFrameDirector" \
    "-DBUCKSWOOD_PREMIERE_FRAME_DIRECTOR=1" \
    "$NATIVE_DIR/resources/BuckswoodFrameDirectorPiPL.r"

build_effect \
    "BuckswoodRadianceRecover" \
    "-DBUCKSWOOD_PREMIERE_RADIANCE_RECOVER=1" \
    "$NATIVE_DIR/resources/BuckswoodRadianceRecoverPiPL.r"

build_effect \
    "BuckswoodTemporalIntegrity" \
    "-DBUCKSWOOD_PREMIERE_TEMPORAL_INTEGRITY=1" \
    "$NATIVE_DIR/resources/BuckswoodTemporalIntegrityPiPL.r"

build_effect \
    "BuckswoodLookDNA" \
    "-DBUCKSWOOD_PREMIERE_LOOK_DNA=1" \
    "$NATIVE_DIR/resources/BuckswoodLookDNAPiPL.r"

python3 - "$INSTALLER_DIR/payload_data.h" \
    photo_prm "$DIST_DIR/BuckswoodAIPhotorealizer.prm" \
    lens_prm "$DIST_DIR/BuckswoodLensPhysics.prm" \
    fake_prm "$DIST_DIR/BuckswoodFakeDiagnostic.prm" \
    film_prm "$DIST_DIR/BuckswoodFilmEmulation.prm" \
    frame_prm "$DIST_DIR/BuckswoodFrameDirector.prm" \
    radiance_prm "$DIST_DIR/BuckswoodRadianceRecover.prm" \
    temporal_prm "$DIST_DIR/BuckswoodTemporalIntegrity.prm" \
    lookdna_prm "$DIST_DIR/BuckswoodLookDNA.prm" <<'PY'
from pathlib import Path
import sys

out = Path(sys.argv[1])
pairs = list(zip(sys.argv[2::2], sys.argv[3::2]))
with out.open("w", encoding="utf-8") as fh:
    fh.write("#pragma once\n#include <cstddef>\n\nnamespace payload {\n\n")
    for name, file_name in pairs:
        data = Path(file_name).read_bytes()
        fh.write(f"const unsigned char {name}[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            fh.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        fh.write("};\n")
        fh.write(f"const std::size_t {name}_size = sizeof({name});\n\n")
    fh.write("} // namespace payload\n")
PY

(
    cd "$INSTALLER_DIR"
    "$WINDRES" --target=pe-x86-64 installer.rc "$BUILD_DIR/premiere_installer.res"
)

"$CXX" --target=x86_64-w64-windows-gnu \
    -std=c++17 \
    -O2 \
    -static \
    -static-libgcc \
    -static-libstdc++ \
    -mwindows \
    -municode \
    -I"$INSTALLER_DIR" \
    "$INSTALLER_DIR/installer.cpp" \
    "$BUILD_DIR/premiere_installer.res" \
    -lshell32 \
    -lole32 \
    -luuid \
    -o "$SETUP_EXE"

"$OBJDUMP" -p "$SETUP_EXE" | grep -E "Subsystem|DLL Name" || true

PLUGIN_NAMES=(
    BuckswoodAIPhotorealizer
    BuckswoodLensPhysics
    BuckswoodFakeDiagnostic
    BuckswoodFilmEmulation
    BuckswoodFrameDirector
    BuckswoodRadianceRecover
    BuckswoodTemporalIntegrity
    BuckswoodLookDNA
)
for name in "${PLUGIN_NAMES[@]}"; do
    cp "$DIST_DIR/$name.prm" "$PACKAGE_DIR/dist/$name.prm"
done
cp "$PORT_DIR/README_PREMIERE_WINDOWS.md" "$PACKAGE_DIR/README_PREMIERE_WINDOWS.md"
cp "$PORT_DIR/PREMIERE_EFFECT_GUIDE.md" "$PACKAGE_DIR/PREMIERE_EFFECT_GUIDE.md"
cp "$NATIVE_DIR/scripts/install_premiere_plugins_windows.bat" "$PACKAGE_DIR/native/scripts/install_premiere_plugins_windows.bat"
cp "$NATIVE_DIR/scripts/build_premiere_windows_plugins.sh" "$PACKAGE_DIR/native/scripts/build_premiere_windows_plugins.sh"
mkdir -p "$PACKAGE_DIR/look_dna"
cp "$ROOT_DIR/Buckswood_Look_DNA/scripts/analyze_reference.py" "$PACKAGE_DIR/look_dna/analyze_reference.py"

(
    cd "$PORT_DIR/release"
    rm -f Buckswood_Premiere_Native_Windows.zip
    zip -r Buckswood_Premiere_Native_Windows.zip Buckswood_Premiere_Native_Windows -x "*/.DS_Store" "__MACOSX/*" "*/._*" >/dev/null
    shasum -a 256 Buckswood_Premiere_Native_Windows.zip Buckswood_Premiere_Plugins_Setup.exe > Buckswood_Premiere_Native_Windows_SHA256SUMS.txt
)

echo
echo "Premiere Windows plugins are in: $DIST_DIR"
echo "Release package:"
echo "$PORT_DIR/release/Buckswood_Premiere_Native_Windows.zip"
echo "$SETUP_EXE"
