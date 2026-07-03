# GitHub Deployment Plan

## Recommended Repository Setup

Use one public repository for the post-production plugins:

```text
buckswood-post-plugins
```

Keep source code and documentation in the repository. Put built installers in GitHub Releases.

Do not publish the full local working folder directly. It contains local build outputs, rendered media, toolchains, and private/third-party reference assets.

## Clean Export

Create a clean publishable copy:

```bash
bash scripts/prepare_github_export.sh
```

The export is written to:

```text
github_export/Buckswood_Post_Plugins
```

Review that folder before uploading.

## First GitHub Publish

From the clean export folder:

```bash
git init
git add .
git commit -m "Initial Buckswood Resolve Plugins release"
git branch -M main
git remote add origin git@github.com:YOUR_USER_OR_ORG/buckswood-resolve-plugins.git
git push -u origin main
```

Then create a GitHub Release named:

```text
v2.0.0
```

Attach:

```text
Buckswood_Resolve_Plugins_v2_macOS.dmg
Buckswood_Resolve_Plugins_v2_macOS.zip
Buckswood_Resolve_Plugins_v2_Windows_Setup.exe
Buckswood_Resolve_Plugins_v2_Windows.zip
Buckswood_Nuke_OFX_v2.zip
Buckswood_Premiere_Native_macOS.zip
Buckswood_Premiere_Native_Windows.zip
Buckswood_Premiere_Plugins_Setup.exe
Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt
```

After `gh auth login`, the local helper can do the push and release upload:

```bash
GH_BIN="/path/to/gh" \
RELEASE_DIR="/path/to/Davinci_plug_in/release" \
scripts/publish_github_release.sh
```

## GitHub Actions

The included workflows do two jobs:

- `ci.yml` runs local smoke tests.
- `release.yml` builds Windows release artifacts on tag pushes and uploads available release assets.

For signed and notarized macOS releases, keep building locally unless the GitHub repository is configured with Apple Developer ID certificate secrets. Never commit Apple passwords, app-specific passwords, certificates, or notary profiles.

Premiere native builds require the Adobe Premiere Pro C++ SDK. Do not commit the SDK. Build Premiere releases locally or configure a private CI step that downloads the SDK from a permitted source.

Nuke packages use the same OFX binaries as the Resolve package and can be rebuilt with:

```bash
bash scripts/build_nuke_ofx_release.sh
```

The current macOS Nuke and Premiere packages are Developer-ID-signed locally. The Windows installers are not Authenticode-signed yet, so SmartScreen warnings are expected.

## Before Making The Repository Public

Check these items:

- Choose a license.
- Remove any private client material.
- Do not include commercial LUTs or paid plugin data.
- Keep Lensfun attribution with any Lensfun-derived profiles.
- Confirm Windows SmartScreen messaging, because the current Windows setup EXE is not Authenticode-signed.
- Clearly label Premiere as beta and Nuke as experimental until both are tested on clean machines.
