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

# waf defaults PREFIX=/usr/local, so binaries land under
# hl2.app/usr/local/. Flatten into the .app root for CFBundleExecutable.
export ANGLE_FRAMEWORK_PATH="$BUILDDIR"
./waf install --destdir="$APP"

flatten_dir() {
	local src="$1"
	[ -d "$src" ] || return 0
	shopt -s nullglob
	local f
	for f in "$src"/*; do
		local base
		base="$(basename "$f")"
		# Prefer the flat copy if a same-named file already exists at root.
		if [ -e "$APP/$base" ] && [ "$f" != "$APP/$base" ]; then
			rm -rf "$APP/$base"
		fi
		mv "$f" "$APP/"
	done
	shopt -u nullglob
	rmdir "$src" 2>/dev/null || rm -rf "$src"
}

# Common PREFIX layouts produced by waf install --destdir
flatten_dir "$APP/usr/local/bin"
flatten_dir "$APP/usr/local/lib"
flatten_dir "$APP/usr/local"
flatten_dir "$APP/usr"
flatten_dir "$APP/bin"
flatten_dir "$APP/lib"

if [ ! -f "$APP/hl2_launcher" ]; then
	echo "Packaging failed: hl2_launcher missing from app root" >&2
	echo "App tree:" >&2
	find "$APP" -maxdepth 4 -print >&2 || true
	exit 1
fi

chmod +x "$APP/hl2_launcher"

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
zip -qr source-engine.ipa Payload

echo "Created $BUILDDIR/source-engine.ipa"
echo "App root contents:"
ls -la "$APP" | head -n 40
