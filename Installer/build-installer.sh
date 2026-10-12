#!/bin/sh
# Builds a universal Release of OpenLFO and wraps it in a macOS installer
# package: dist/OpenLFO-<version>-macOS.pkg
#
# Optional signing, for a package that opens without a Gatekeeper warning:
#   SIGN_APP="Developer ID Application: ..."     signs the plugins and the app
#                                                (ad-hoc signed otherwise)
#   SIGN_PKG="Developer ID Installer: ..."       signs the package
#   NOTARY_PROFILE=<notarytool keychain profile> notarises and staples it
set -eu
cd "$(dirname "$0")/.."

NAME=OpenLFO
ID=com.JezzterInc.OpenLFO.installer
VERSION=$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)
BUILD=cmake-build-installer
ARTEFACTS=$BUILD/${NAME}_artefacts/Release
STAGE=$BUILD/package
ROOT=$STAGE/root
OUTPUT=dist/$NAME-$VERSION-macOS.pkg

cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build "$BUILD"

# Lay the files out as they should land on the user's disk.
rm -rf "$STAGE"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" \
  "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Applications" \
  "$STAGE/scripts" "$STAGE/resources" dist
ditto "$ARTEFACTS/VST3/$NAME.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
ditto "$ARTEFACTS/AU/$NAME.component" \
  "$ROOT/Library/Audio/Plug-Ins/Components/$NAME.component"
ditto "$ARTEFACTS/Standalone/$NAME.app" "$ROOT/Applications/$NAME.app"

# Ad-hoc signatures unless a Developer ID is given.
if [ -n "${SIGN_APP:-}" ]; then
  set -- --options runtime --timestamp -s "$SIGN_APP"
else
  set -- -s -
fi
codesign --force "$@" "$ROOT/Library/Audio/Plug-Ins/VST3/$NAME.vst3" \
  "$ROOT/Library/Audio/Plug-Ins/Components/$NAME.component"
codesign --force "$@" --entitlements Installer/standalone.entitlements \
  "$ROOT/Applications/$NAME.app"

# Without this the installer "relocates" a bundle onto any other copy of it
# that Spotlight knows about, such as one in a build folder.
pkgbuild --analyze --root "$ROOT" "$STAGE/components.plist" >/dev/null
for index in 0 1 2; do
  /usr/libexec/PlistBuddy -c "Delete :$index:BundleIsRelocatable" \
    "$STAGE/components.plist" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Add :$index:BundleIsRelocatable bool false" \
    "$STAGE/components.plist"
done

# Make hosts rescan Audio Units straight away.
cat > "$STAGE/scripts/postinstall" <<'SCRIPT'
#!/bin/sh
killall -9 AudioComponentRegistrar 2>/dev/null
exit 0
SCRIPT
chmod +x "$STAGE/scripts/postinstall"

pkgbuild --root "$ROOT" --component-plist "$STAGE/components.plist" \
  --scripts "$STAGE/scripts" --identifier "$ID" --version "$VERSION" \
  --install-location / "$STAGE/$NAME-files.pkg" >/dev/null

cp LICENSE "$STAGE/resources/LICENSE.txt"
cat > "$STAGE/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$NAME $VERSION</title>
  <license file="LICENSE.txt"/>
  <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="true"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <volume-check>
    <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
  </volume-check>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default" title="$NAME"><pkg-ref id="$ID"/></choice>
  <pkg-ref id="$ID" version="$VERSION">$NAME-files.pkg</pkg-ref>
</installer-gui-script>
XML

rm -f "$OUTPUT"
if [ -n "${SIGN_PKG:-}" ]; then
  productbuild --distribution "$STAGE/distribution.xml" \
    --package-path "$STAGE" --resources "$STAGE/resources" \
    --sign "$SIGN_PKG" --timestamp "$OUTPUT" >/dev/null
else
  productbuild --distribution "$STAGE/distribution.xml" \
    --package-path "$STAGE" --resources "$STAGE/resources" "$OUTPUT" >/dev/null
fi

if [ -n "${NOTARY_PROFILE:-}" ]; then
  xcrun notarytool submit "$OUTPUT" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUTPUT"
fi

echo "Built $OUTPUT"
