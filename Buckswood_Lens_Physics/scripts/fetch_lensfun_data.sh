#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSETS_DIR="$ROOT_DIR/assets"
REPO_DIR="$ASSETS_DIR/lensfun_repo"

mkdir -p "$ASSETS_DIR"

if [[ ! -d "$REPO_DIR/.git" ]]; then
  git clone --depth 1 --filter=blob:none --sparse https://github.com/lensfun/lensfun.git "$REPO_DIR"
  git -C "$REPO_DIR" sparse-checkout set data
else
  git -C "$REPO_DIR" pull --ff-only
fi

COMMIT="$(git -C "$REPO_DIR" rev-parse HEAD)"
python3 "$ROOT_DIR/scripts/import_lensfun_profiles.py" --source-commit "$COMMIT"

