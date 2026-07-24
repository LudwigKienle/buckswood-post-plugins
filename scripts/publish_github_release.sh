#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GH_BIN="${GH_BIN:-gh}"
REPO_NAME="${REPO_NAME:-buckswood-post-plugins}"
VISIBILITY="${VISIBILITY:-public}"
TAG="${TAG:-v2.4.0}"
TITLE="${TITLE:-Buckswood Post Plugins v2.4.0}"
RELEASE_DIR="${RELEASE_DIR:-}"
NOTES_FILE="${NOTES_FILE:-$ROOT_DIR/docs/RELEASE_NOTES_v2.4.0.md}"
PRERELEASE="${PRERELEASE:-false}"

if [[ -z "$RELEASE_DIR" ]]; then
    if [[ -d "$ROOT_DIR/release" ]]; then
        RELEASE_DIR="$ROOT_DIR/release"
    elif [[ -d "$ROOT_DIR/../../release" ]]; then
        RELEASE_DIR="$(cd "$ROOT_DIR/../../release" && pwd)"
    else
        echo "Set RELEASE_DIR to the folder containing release assets." >&2
        exit 1
    fi
fi

"$GH_BIN" auth status >/dev/null

CURRENT_BRANCH="$(git branch --show-current)"
[[ -n "$CURRENT_BRANCH" ]] || {
    echo "Publish from a named branch, not detached HEAD." >&2
    exit 1
}

if ! git remote get-url origin >/dev/null 2>&1; then
    if "$GH_BIN" repo view "$REPO_NAME" >/dev/null 2>&1; then
        git remote add origin "$("$GH_BIN" repo view "$REPO_NAME" --json sshUrl --jq .sshUrl)"
        git push -u origin "$CURRENT_BRANCH"
    else
        if [[ "$VISIBILITY" == "private" ]]; then
            "$GH_BIN" repo create "$REPO_NAME" --private --source "$ROOT_DIR" --remote origin --push
        else
            "$GH_BIN" repo create "$REPO_NAME" --public --source "$ROOT_DIR" --remote origin --push
        fi
    fi
else
    git push -u origin "$CURRENT_BRANCH"
fi

assets=(
    "$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_macOS.dmg"
    "$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_macOS.zip"
    "$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_Windows_Setup.exe"
    "$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_Windows.zip"
    "$RELEASE_DIR/Buckswood_Film_Emulation_v2_Installer.dmg"
    "$RELEASE_DIR/Buckswood_Film_Emulation_v2_Installer.pkg"
    "$RELEASE_DIR/Buckswood_Cinematic_Tools_v2_Installer.dmg"
    "$RELEASE_DIR/Buckswood_Cinematic_Tools_v2_Installer.pkg"
    "$RELEASE_DIR/Buckswood_Look_DNA_v2_Installer.dmg"
    "$RELEASE_DIR/Buckswood_Look_DNA_v2_Installer.pkg"
    "$RELEASE_DIR/Buckswood_Nuke_OFX_v2.zip"
    "$RELEASE_DIR/Buckswood_Premiere_Native_macOS.zip"
    "$RELEASE_DIR/Buckswood_Premiere_Native_Windows.zip"
    "$RELEASE_DIR/Buckswood_Premiere_Plugins_Setup.exe"
    "$RELEASE_DIR/Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt"
)

existing_assets=()
for asset in "${assets[@]}"; do
    [[ -f "$asset" ]] || { echo "Missing release asset: $asset" >&2; exit 1; }
    existing_assets+=("$asset")
done

release_flags=()
if [[ "$PRERELEASE" == "true" ]]; then
    release_flags+=(--prerelease)
fi

if "$GH_BIN" release view "$TAG" >/dev/null 2>&1; then
    "$GH_BIN" release edit "$TAG" \
        --title "$TITLE" \
        --notes-file "$NOTES_FILE" \
        "${release_flags[@]}"
    "$GH_BIN" release upload "$TAG" "${existing_assets[@]}" --clobber
else
    "$GH_BIN" release create "$TAG" "${existing_assets[@]}" \
        --title "$TITLE" \
        --notes-file "$NOTES_FILE" \
        "${release_flags[@]}"
fi

"$GH_BIN" repo view --web >/dev/null 2>&1 || true
echo "Published GitHub repository and release: $TAG"
