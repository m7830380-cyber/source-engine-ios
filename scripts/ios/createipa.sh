#!/bin/bash
set -euo pipefail

cd "${0%/*}/../.."
ROOT="$PWD"
BUILDDIR="$ROOT/build/ios"
APP="$BUILDDIR/hl2.app"
BUNDLE="$ROOT/scripts/ios/bundle"

mkdir -p "$APP/Frameworks"

cp "$BUNDLE/Info.plist" "$APP/"
cp "$BUNDLE/LaunchScreen.storyboard" "$APP/"
cp "$BUNDLE/extras_dir.vpk" "$APP/"

for fw in SDL2 libEGL libGLESv2; do
	if [ ! -d "$BUILDDIR/${fw}.framework" ]; then
		echo "Missing $BUILDDIR/${fw}.framework" >&2
		exit 1
	fi
	cp -R "$BUILDDIR/${fw}.framework" "$APP/Frameworks/"
done

export ANGLE_FRAMEWORK_PATH="$BUILDDIR"
./waf install --destdir="$APP"

rm -rf "$BUILDDIR/Payload"
mkdir -p "$BUILDDIR/Payload"
cp -a "$APP" "$BUILDDIR/Payload/"

codesign --entitlements "$ROOT/scripts/ios/entitlements.plist" \
	--sign "-" --force --deep "$BUILDDIR/Payload/hl2.app"

cd "$BUILDDIR"
rm -f source-engine.ipa
zip -qr source-engine.ipa Payload

echo "Created $BUILDDIR/source-engine.ipa"
