#!/bin/bash
# Builds dist/GateDaddy-<version>-mac.pkg (installs AU, VST3 and the standalone app)
# and dist/GateDaddy-<version>-mac.zip (raw bundles) from a finished Release build.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^project(GateDaddy VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)
ART="build/GateDaddy_artefacts/Release"
STAGE="build/pkgroot"
rm -rf "$STAGE" dist && mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Library/Audio/Plug-Ins/Components" "$STAGE/Applications" dist

cp -R "$ART/VST3/Gate Daddy.vst3" "$STAGE/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/Gate Daddy.component" "$STAGE/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/Standalone/Gate Daddy.app" "$STAGE/Applications/"

# Stop the installer from "helpfully" updating a copy the user moved elsewhere.
xattr -cr "$STAGE"
export COPYFILE_DISABLE=1
pkgbuild --analyze --root "$STAGE" build/components.plist
python3 - <<'PY'
import plistlib
path = "build/components.plist"
with open(path, "rb") as f:
    items = plistlib.load(f)
def fix(entries):
    for e in entries:
        e["BundleIsRelocatable"] = False
        fix(e.get("ChildBundles", []))
fix(items)
with open(path, "wb") as f:
    plistlib.dump(items, f)
PY

pkgbuild --root "$STAGE" --component-plist build/components.plist --install-location / \
         --identifier com.gatedaddylabs.gatedaddy --version "$VERSION" \
         "dist/GateDaddy-$VERSION-mac.pkg"

(cd "$STAGE" && zip -qry "../../dist/GateDaddy-$VERSION-mac.zip" Library Applications)
ls -lh dist
