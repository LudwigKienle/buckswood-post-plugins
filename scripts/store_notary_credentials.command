#!/usr/bin/env bash
set -euo pipefail

PROFILE_NAME="${NOTARY_PROFILE:-BuckswoodNotary}"
APPLE_ID="${APPLE_ID:-}"
TEAM_ID="${TEAM_ID:-}"

if [[ -z "$APPLE_ID" || -z "$TEAM_ID" ]]; then
    echo "Set APPLE_ID and TEAM_ID before running this script."
    echo
    echo "Example:"
    echo "APPLE_ID=\"you@example.com\" TEAM_ID=\"ABCDE12345\" ./scripts/store_notary_credentials.command"
    echo
    read -r -p "Press Return to close this window..."
    exit 1
fi

echo "Buckswood Notary Credential Setup"
echo "================================="
echo
echo "This stores your Apple notarization credential securely in the macOS Keychain."
echo "Use an app-specific password, not your normal Apple ID password."
echo
echo "Apple ID: $APPLE_ID"
echo "Team ID:  $TEAM_ID"
echo "Profile:  $PROFILE_NAME"
echo
xcrun notarytool store-credentials "$PROFILE_NAME" \
    --apple-id "$APPLE_ID" \
    --team-id "$TEAM_ID"

echo
echo "Saved profile: $PROFILE_NAME"
echo "You can now run:"
echo "NOTARY_PROFILE=\"$PROFILE_NAME\" ./scripts/notarize_public_release.sh"
echo
read -r -p "Press Return to close this window..."
