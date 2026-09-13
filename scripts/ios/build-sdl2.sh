#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"
OUT="$ROOT/build/ios"
SDL_SRC="$ROOT/thirdparty/SDL-src"

mkdir -p "$OUT"

if [ ! -d "$SDL_SRC/Xcode/SDL/SDL.xcodeproj" ]; then
	echo "SDL source not found at $SDL_SRC - run git submodule update first" >&2
	exit 1
fi

xcodebuild -project "$SDL_SRC/Xcode/SDL/SDL.xcodeproj" \
	-scheme "Framework-iOS" \
	-configuration Release \
	-sdk iphoneos \
	-derivedDataPath "$OUT/sdl2-derived" \
	ONLY_ACTIVE_ARCH=NO \
	BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
	CODE_SIGNING_ALLOWED=NO \
	CODE_SIGNING_REQUIRED=NO

FRAMEWORK="$(find "$OUT/sdl2-derived" -path '*SDL2.framework' -type d | head -n1)"
if [ -z "$FRAMEWORK" ]; then
	echo "xcodebuild did not produce SDL2.framework, falling back to reference IPA..." >&2
	TMP="$(mktemp -d)"
	IPA_URL="${REFERENCE_IPA_URL:-https://github.com/ksagameng2/source-engine/releases/download/test/source-engine.ipa}"
	curl -fsSL -o "$TMP/ref.ipa" "$IPA_URL"
	unzip -q "$TMP/ref.ipa" "Payload/hl2.app/Frameworks/SDL2.framework/*" -d "$TMP"
	FRAMEWORK="$TMP/Payload/hl2.app/Frameworks/SDL2.framework"
fi

rm -rf "$OUT/SDL2.framework"
cp -R "$FRAMEWORK" "$OUT/SDL2.framework"
echo "SDL2.framework installed to $OUT/SDL2.framework"
