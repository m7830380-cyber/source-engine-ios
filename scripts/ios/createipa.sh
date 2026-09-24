#!/bin/bash
set -euo pipefail

cd "${0%/*}/../.."
ROOT="$PWD"
BUILDDIR="$ROOT/build/ios"
APP_BUNDLE="${IOS_APP_BUNDLE:-hl2.app}"
APP="$BUILDDIR/$APP_BUNDLE"
BUNDLE="$ROOT/scripts/ios/bundle"
BUNDLE_ID="${IOS_BUNDLE_ID:-com.sourceengine.port}"
DISPLAY_NAME="${IOS_DISPLAY_NAME:-source-engine}"
IPA_FILE="${IOS_IPA_FILE:-source-engine.ipa}"

rm -rf "$APP"
mkdir -p "$APP/Frameworks"

cp "$BUNDLE/Info.plist" "$APP/"
cp "$BUNDLE/LaunchScreen.storyboard" "$APP/"
# touch control textures (materials/vgui/touch/*), from the source-engine port
cp "$BUNDLE/extras_dir.vpk" "$APP/"

if [ -x /usr/libexec/PlistBuddy ]; then
	/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP/Info.plist"
	/usr/libexec/PlistBuddy -c "Set :CFBundleName $DISPLAY_NAME" "$APP/Info.plist"
	if /usr/libexec/PlistBuddy -c "Print :CFBundleDisplayName" "$APP/Info.plist" >/dev/null 2>&1; then
		/usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $DISPLAY_NAME" "$APP/Info.plist"
	else
		/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string $DISPLAY_NAME" "$APP/Info.plist"
	fi
fi

for fw in SDL2 libEGL libGLESv2; do
	if [ ! -d "$BUILDDIR/${fw}.framework" ]; then
		echo "Missing $BUILDDIR/${fw}.framework" >&2
		exit 1
	fi
	rm -rf "$APP/Frameworks/${fw}.framework"
	cp -R "$BUILDDIR/${fw}.framework" "$APP/Frameworks/"
done

# Reject link-only stubs — these crash at launch with dyld "image not found".
if [ ! -f "$APP/Frameworks/SDL2.framework/SDL2" ]; then
	echo "Packaging failed: SDL2.framework has no SDL2 Mach-O binary (got a .tbd stub?)" >&2
	ls -la "$APP/Frameworks/SDL2.framework" >&2 || true
	exit 1
fi
if file -b "$APP/Frameworks/SDL2.framework/SDL2" | grep -qi tbd; then
	echo "Packaging failed: SDL2.framework/SDL2 is a TBD stub" >&2
	exit 1
fi
for fw in libEGL libGLESv2; do
	if [ ! -f "$APP/Frameworks/${fw}.framework/${fw}" ]; then
		echo "Packaging failed: missing $fw.framework/$fw" >&2
		exit 1
	fi
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
		if [ -e "$APP/$base" ] && [ "$f" != "$APP/$base" ]; then
			rm -rf "$APP/$base"
		fi
		mv "$f" "$APP/"
	done
	shopt -u nullglob
	rmdir "$src" 2>/dev/null || rm -rf "$src"
}

flatten_dir "$APP/usr/local/bin"
flatten_dir "$APP/usr/local/lib"
flatten_dir "$APP/usr/local"
flatten_dir "$APP/usr"
flatten_dir "$APP/bin"
flatten_dir "$APP/lib"

if [ ! -f "$APP/csgo_osx64" ]; then
	echo "Packaging failed: csgo_osx64 missing from app root" >&2
	echo "App tree:" >&2
	find "$APP" -maxdepth 4 -print >&2 || true
	exit 1
fi

chmod +x "$APP/csgo_osx64"

# Game content (csgo/ from a CS:GO install) goes in the app's Documents
# directory, which launcher_main uses as the base directory.

# Rewrite absolute CI/build load paths to @rpath so the IPA runs on device.
# Also normalize ANGLE framework load paths to match the reference IPA.
fix_macho() {
	local bin="$1"
	[ -f "$bin" ] || return 0

	if [[ "$bin" == *.dylib ]]; then
		install_name_tool -id "@rpath/$(basename "$bin")" "$bin" 2>/dev/null || true
	fi

	install_name_tool -add_rpath "@executable_path" "$bin" 2>/dev/null || true
	install_name_tool -add_rpath "@executable_path/Frameworks" "$bin" 2>/dev/null || true

	# otool -L: skip the first identity line via awk NR>1.
	local dep
	while IFS= read -r dep; do
		dep="${dep%% (*}"
		dep="$(echo "$dep" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
		[ -n "$dep" ] || continue

		case "$dep" in
			/System/*|/usr/lib/*|/usr/local/lib/*)
				;;
			@rpath/libEGL)
				install_name_tool -change "$dep" "@rpath/libEGL.framework/libEGL" "$bin" 2>/dev/null || true
				;;
			@rpath/libGLESv2)
				install_name_tool -change "$dep" "@rpath/libGLESv2.framework/libGLESv2" "$bin" 2>/dev/null || true
				;;
			/*/*.dylib|/*.dylib)
				# Absolute build/CI paths baked by the linker.
				install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$bin" 2>/dev/null || true
				;;
		esac
	done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}')
}

if command -v install_name_tool >/dev/null && command -v otool >/dev/null; then
	fix_macho "$APP/csgo_osx64"
	shopt -s nullglob
	for dylib in "$APP"/*.dylib; do
		fix_macho "$dylib"
	done
	shopt -u nullglob

	# Fail the pack if any LC_LOAD_DYLIB still points at the CI workspace.
	# otool -L prints "<path>:" headers — only check indented dependency lines.
	bad_deps="$(otool -L "$APP"/*.dylib "$APP/csgo_osx64" 2>/dev/null | awk '/^\t/ {print $1}' | grep -E '^/Users/|/build/' || true)"
	if [ -n "$bad_deps" ]; then
		echo "Packaging failed: absolute build paths still present in load commands:" >&2
		echo "$bad_deps" >&2
		exit 1
	fi
fi

rm -rf "$BUILDDIR/Payload"
mkdir -p "$BUILDDIR/Payload"
cp -a "$APP" "$BUILDDIR/Payload/"

codesign --entitlements "$ROOT/scripts/ios/entitlements.plist" \
	--sign "-" --force --deep "$BUILDDIR/Payload/$APP_BUNDLE"

cd "$BUILDDIR"
rm -f "$IPA_FILE"
zip -qr "$IPA_FILE" Payload

echo "Created $BUILDDIR/$IPA_FILE"
echo "Bundle ID: $BUNDLE_ID"
echo "Display name: $DISPLAY_NAME"
echo "App root contents:"
ls -la "$APP" | head -n 40
echo "SDL2 framework:"
ls -la "$APP/Frameworks/SDL2.framework" | head -n 20
