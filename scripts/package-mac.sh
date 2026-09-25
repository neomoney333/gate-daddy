#!/bin/bash
# Builds "dist/Install Gate Daddy <version>.pkg" (installs AU, VST3 and the standalone app)
# and dist/GateDaddy-<version>-mac.zip (raw bundles) from a finished Release build.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^project(GateDaddy VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)
ART="build/GateDaddy_artefacts/Release"
STAGE="build/pkgroot"
rm -rf "$STAGE" build/pkgparts dist && mkdir -p "$STAGE/Library/Audio/Plug-Ins/VST3" "$STAGE/Library/Audio/Plug-Ins/Components" "$STAGE/Applications" dist

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

mkdir -p build/pkgparts
pkgbuild --root "$STAGE" --component-plist build/components.plist --install-location / \
         --identifier com.gatedaddylabs.gatedaddy --version "$VERSION" build/pkgparts/core.pkg

cat > build/distribution.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Gate Daddy ${VERSION}</title>
    <welcome mime-type="text/plain"><![CDATA[GATE DADDY ${VERSION}: tough love rhythm therapy.

This installs:
  • Gate Daddy (VST3) → /Library/Audio/Plug-Ins/VST3
  • Gate Daddy (Audio Unit) → /Library/Audio/Plug-Ins/Components
  • Gate Daddy (standalone app) → /Applications

Quit Ableton Live before installing. Dr. Gate will wait.]]></welcome>
    <conclusion mime-type="text/plain"><![CDATA[Installed. Now get real.

Open Ableton Live → Settings → Plug-ins → turn on "Use VST3 Plug-In System Folders" and "Use Audio Units v2" → hold Option and click Rescan.

Gate Daddy appears under Plug-ins → Gate Daddy Labs.]]></conclusion>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline><line choice="default"><line choice="core"/></line></choices-outline>
    <choice id="default"/>
    <choice id="core" visible="false"><pkg-ref id="com.gatedaddylabs.gatedaddy"/></choice>
    <pkg-ref id="com.gatedaddylabs.gatedaddy" version="${VERSION}" onConclusion="none">core.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution build/distribution.xml --package-path build/pkgparts "dist/Install Gate Daddy ${VERSION}.pkg"

(cd "$STAGE" && zip -qry "../../dist/GateDaddy-$VERSION-mac.zip" Library Applications)
ls -lh dist
