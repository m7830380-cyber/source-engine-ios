#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"
OUT="$ROOT/build/ios"
SDL_SRC="$ROOT/thirdparty/SDL-src"

mkdir -p "$OUT"

fetch_sdl2_from_reference_ipa() {
	local tmp ipa_url framework
	tmp="$(mktemp -d)"
	ipa_url="${REFERENCE_IPA_URL:-https://github.com/ksagameng2/source-engine/releases/download/test/source-engine.ipa}"
	curl -fsSL -o "$tmp/ref.ipa" "$ipa_url"
	unzip -q "$tmp/ref.ipa" "Payload/hl2.app/Frameworks/SDL2.framework/*" -d "$tmp"
	framework="$tmp/Payload/hl2.app/Frameworks/SDL2.framework"
	rm -rf "$OUT/SDL2.framework"
	cp -R "$framework" "$OUT/SDL2.framework"
	rm -rf "$tmp"
}

if [ ! -d "$SDL_SRC/Xcode/SDL/SDL.xcodeproj" ]; then
	echo "SDL source not found at $SDL_SRC - using reference IPA framework" >&2
	fetch_sdl2_from_reference_ipa
	echo "SDL2.framework installed to $OUT/SDL2.framework"
	exit 0
fi

if ! xcodebuild -project "$SDL_SRC/Xcode/SDL/SDL.xcodeproj" \
	-scheme "Framework-iOS" \
	-configuration Release \
	-sdk iphoneos \
	-derivedDataPath "$OUT/sdl2-derived" \
	ONLY_ACTIVE_ARCH=NO \
	BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
	IPHONEOS_DEPLOYMENT_TARGET=12.0 \
	CODE_SIGNING_ALLOWED=NO \
	CODE_SIGNING_REQUIRED=NO; then
	echo "SDL2 xcodebuild failed; falling back to reference IPA framework" >&2
	fetch_sdl2_from_reference_ipa
	echo "SDL2.framework installed to $OUT/SDL2.framework"
	exit 0
fi

FRAMEWORK="$(find "$OUT/sdl2-derived" -path '*SDL2.framework' -type d | head -n1)"
if [ -z "$FRAMEWORK" ]; then
	echo "Failed to locate built SDL2.framework; using reference IPA framework" >&2
	fetch_sdl2_from_reference_ipa
	echo "SDL2.framework installed to $OUT/SDL2.framework"
	exit 0
fi

rm -rf "$OUT/SDL2.framework"
cp -R "$FRAMEWORK" "$OUT/SDL2.framework"
echo "SDL2.framework installed to $OUT/SDL2.framework"
