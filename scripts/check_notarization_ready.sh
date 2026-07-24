#!/usr/bin/env bash
set -euo pipefail

PROFILE_NAME="${NOTARY_PROFILE:-BuckswoodNotary}"
TEAM_ID="${TEAM_ID:-}"

echo "Buckswood notarization readiness"
echo "================================"
echo

APP_IDENTITY="$(security find-identity -v -p codesigning | awk -F '"' -v team="$TEAM_ID" '$2 ~ /Developer ID Application:/ && (team == "" || $2 ~ team) { print $2; exit }')"
INSTALLER_CERT="$(security find-certificate -c "Developer ID Installer" -a -Z 2>/dev/null | awk -F '"' -v team="$TEAM_ID" '$4 ~ /Developer ID Installer:/ && (team == "" || $4 ~ team) { print $4; exit }')"

if [[ -n "$APP_IDENTITY" ]]; then
    echo "OK   Developer ID Application: $APP_IDENTITY"
else
    echo "MISS Developer ID Application"
fi

if [[ -n "$INSTALLER_CERT" ]]; then
    echo "OK   Developer ID Installer:   $INSTALLER_CERT"
else
    echo "MISS Developer ID Installer"
fi

if xcrun notarytool history --keychain-profile "$PROFILE_NAME" >/dev/null 2>&1; then
    echo "OK   Notary profile:           $PROFILE_NAME"
else
    echo "MISS Notary profile:           $PROFILE_NAME"
fi

echo
if [[ -n "$APP_IDENTITY" && -n "$INSTALLER_CERT" ]] && xcrun notarytool history --keychain-profile "$PROFILE_NAME" >/dev/null 2>&1; then
    echo "Ready: run ./scripts/notarize_public_release.sh"
else
    echo "Not ready yet."
fi
