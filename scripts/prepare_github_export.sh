#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXPORT_PARENT="$ROOT_DIR/github_export"
EXPORT_DIR="$EXPORT_PARENT/Buckswood_Post_Plugins"

rm -rf "$EXPORT_DIR"
mkdir -p "$EXPORT_PARENT"

rsync -a --delete \
  --exclude ".git/" \
  --exclude ".DS_Store" \
  --exclude "._*" \
  --exclude "__MACOSX/" \
  --exclude ".claude/" \
  --exclude ".codex/" \
  --exclude ".idea/" \
  --exclude ".vscode/" \
  --exclude "build/" \
  --exclude "dist/" \
  --exclude "release/" \
  --exclude "public_release/" \
  --exclude "packaging/pkgroot/" \
  --exclude "packaging/dmg_stage/" \
  --exclude "github_export/" \
  --exclude "_feedback_video_frames/" \
  --exclude "disabled_resolve_buckswood_backups/" \
  --exclude "Backup/" \
  --exclude "Referent Viewing LUT - CKC/" \
  --exclude "third_party/toolchains/" \
  --exclude "windows_release/build/" \
  --exclude "windows_release/public/" \
  --exclude "windows_release/Buckswood_Resolve_Plugins_Windows/" \
  --exclude "windows_release/installer/payload_data.h" \
  --exclude "premiere_port/windows_installer/payload_data.h" \
  --exclude "Buckswood_YouTube_Giveaway_HeyGen/renders/" \
  --exclude "Buckswood_YouTube_Giveaway_HeyGen/" \
  --exclude "Buckswood_AutoGrade_Assistant/output/" \
  --exclude "nuke_release/" \
  --exclude "premiere_port/release/*.mp4" \
  --exclude "MOU_AI_Film_Pipeline_*.docx" \
  --exclude "gemini-code-*.md" \
  --exclude "*.dmg" \
  --exclude "*.pkg" \
  --exclude "*.zip" \
  --exclude "*.exe" \
  --exclude "*.ofx" \
  --exclude "*.ofx.bundle/" \
  --exclude "*.prm" \
  "$ROOT_DIR/" "$EXPORT_DIR/"

find "$EXPORT_DIR" -name ".DS_Store" -delete
find "$EXPORT_DIR" -name "._*" -delete
find "$EXPORT_DIR" -type d -name ".git" -prune -exec rm -rf {} +

rm -rf \
  "$EXPORT_DIR/third_party/openfx/Documentation" \
  "$EXPORT_DIR/third_party/openfx/Examples" \
  "$EXPORT_DIR/third_party/openfx/HostSupport" \
  "$EXPORT_DIR/third_party/openfx/Support" \
  "$EXPORT_DIR/third_party/openfx/cmake" \
  "$EXPORT_DIR/third_party/openfx/scripts" \
  "$EXPORT_DIR/third_party/openfx/test_package"

echo "GitHub export ready:"
echo "$EXPORT_DIR"
