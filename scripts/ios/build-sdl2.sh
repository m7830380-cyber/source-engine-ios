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
	if [ ! -f "$framework/SDL2" ]; then
		echo "Reference IPA is missing SDL2.framework/SDL2" >&2
		rm -rf "$tmp"
		exit 1
	fi
	rm -rf "$OUT/SDL2.framework"
	cp -R "$framework" "$OUT/SDL2.framework"
	rm -rf "$tmp"
}

is_macho_binary() {
	local path="$1"
	[ -f "$path" ] || return 1
	# Reject TBD stubs and empty placeholders.
	case "$(basename "$path")" in
		*.tbd) return 1 ;;
	esac
	file -b "$path" 2>/dev/null | grep -Eq 'Mach-O|shared library|dynamically linked'
}

install_framework() {
	local framework="$1"
	if ! is_macho_binary "$framework/SDL2"; then
		return 1
	fi
	rm -rf "$OUT/SDL2.framework"
	cp -R "$framework" "$OUT/SDL2.framework"
	# Drop link-only TBD stubs so the packaged framework can't be mistaken for complete.
	find "$OUT/SDL2.framework" -name '*.tbd' -delete 2>/dev/null || true
	echo "SDL2.framework installed to $OUT/SDL2.framework ($(wc -c < "$OUT/SDL2.framework/SDL2") bytes)"
	return 0
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
	IPHONEOS_DEPLOYMENT_TARGET=12.0 \
	TARGETED_DEVICE_FAMILY=1,2 \
	CODE_SIGNING_ALLOWED=NO \
	CODE_SIGNING_REQUIRED=NO; then
	echo "SDL2 xcodebuild failed; falling back to reference IPA framework" >&2
	fetch_sdl2_from_reference_ipa
	echo "SDL2.framework installed to $OUT/SDL2.framework"
	exit 0
fi

# Prefer the real Products framework. EagerLinkingTBDs often only contains SDL2.tbd
# and was previously copied as the entire "framework", which crashes at launch.
FRAMEWORK=""
for candidate in \
	"$OUT/sdl2-derived/Build/Products/Release-iphoneos/SDL2.framework" \
	"$OUT/sdl2-derived/Build/Products/Release-iphonesimulator/SDL2.framework"
do
	if is_macho_binary "$candidate/SDL2"; then
		FRAMEWORK="$candidate"
		break
	fi
done

if [ -z "$FRAMEWORK" ]; then
	while IFS= read -r bin; do
		if is_macho_binary "$bin"; then
			FRAMEWORK="$(dirname "$bin")"
			break
		fi
	done < <(find "$OUT/sdl2-derived/Build/Products" -path '*/SDL2.framework/SDL2' -type f 2>/dev/null)
fi

if [ -n "$FRAMEWORK" ] && install_framework "$FRAMEWORK"; then
	exit 0
fi

echo "Built SDL2.framework missing Mach-O binary; using reference IPA framework" >&2
fetch_sdl2_from_reference_ipa
echo "SDL2.framework installed to $OUT/SDL2.framework"
