#!/bin/bash
set -euo pipefail

cd "${0%/*}/../.."
ROOT="$PWD"
BUILDDIR="$ROOT/build/ios"
APP="$BUILDDIR/hl2.app"
BUNDLE="$ROOT/scripts/ios/bundle"

rm -rf "$APP"
mkdir -p "$APP/Frameworks"

cp "$BUNDLE/Info.plist" "$APP/"
cp "$BUNDLE/LaunchScreen.storyboard" "$APP/"
cp "$BUNDLE/extras_dir.vpk" "$APP/"

for fw in SDL2 libEGL libGLESv2; do
	if [ ! -d "$BUILDDIR/${fw}.framework" ]; then
		echo "Missing $BUILDDIR/${fw}.framework" >&2
		exit 1
	fi
	rm -rf "$APP/Frameworks/${fw}.framework"
	cp -R "$BUILDDIR/${fw}.framework" "$APP/Frameworks/"
done

# Install into a staging tree, then flatten into the .app root.
# waf defaults PREFIX to /usr/local, which otherwise nests binaries under
# hl2.app/usr/local/ and breaks CFBundleExecutable lookup.
STAGE="$BUILDDIR/install-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

export ANGLE_FRAMEWORK_PATH="$BUILDDIR"
./waf install --destdir="$STAGE"

# Collect installed binaries from common PREFIX layouts into the app root.
shopt -s nullglob
for candidate in \
	"$STAGE" \
	"$STAGE/usr/local" \
	"$STAGE/usr/local/bin" \
	"$STAGE/usr/local/lib" \
	"$STAGE/bin" \
	"$STAGE/lib"
do
	[ -d "$candidate" ] || continue
	for f in "$candidate"/*; do
		base="$(basename "$f")"
		case "$base" in
			Frameworks|Payload|_CodeSignature|Info.plist|LaunchScreen.storyboard|extras_dir.vpk|usr|bin|lib|share|include)
				continue
				;;
		esac
		if [ -f "$f" ] || [ -L "$f" ]; then
			cp -a "$f" "$APP/"
		fi
	done
done
shopt -u nullglob

if [ ! -f "$APP/hl2_launcher" ]; then
	echo "Packaging failed: hl2_launcher missing from app root" >&2
	echo "Stage tree:" >&2
	find "$STAGE" -maxdepth 4 -print >&2 || true
	exit 1
fi

chmod +x "$APP/hl2_launcher"

# Ensure dylibs are loadable next to the executable.
if command -v install_name_tool >/dev/null; then
	for dylib in "$APP"/lib*.dylib; do
		[ -f "$dylib" ] || continue
		install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib" 2>/dev/null || true
	done
	install_name_tool -add_rpath "@executable_path" "$APP/hl2_launcher" 2>/dev/null || true
	install_name_tool -add_rpath "@executable_path/Frameworks" "$APP/hl2_launcher" 2>/dev/null || true
fi

rm -rf "$BUILDDIR/Payload"
mkdir -p "$BUILDDIR/Payload"
cp -a "$APP" "$BUILDDIR/Payload/"

codesign --entitlements "$ROOT/scripts/ios/entitlements.plist" \
	--sign "-" --force --deep "$BUILDDIR/Payload/hl2.app"

cd "$BUILDDIR"
rm -f source-engine.ipa
# Store paths with Unix separators; keep Payload/hl2.app/... at archive root.
zip -qr source-engine.ipa Payload

echo "Created $BUILDDIR/source-engine.ipa"
echo "App root contents:"
ls -la "$APP" | head -n 40
