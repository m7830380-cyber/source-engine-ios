#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"
OUT="$ROOT/build/ios"

mkdir -p "$OUT"

if [ -d "$OUT/libEGL.framework" ] && [ -d "$OUT/libGLESv2.framework" ]; then
	echo "ANGLE frameworks already present in $OUT"
	exit 0
fi

# ANGLE is built separately from the engine; reuse known-good Metal/EGL
# frameworks from the upstream reference IPA to keep CI build times reasonable.
IPA_URL="${REFERENCE_IPA_URL:-https://github.com/ksagameng2/source-engine/releases/download/test/source-engine.ipa}"
TMP="$(mktemp -d)"

cleanup() {
	rm -rf "$TMP"
}
trap cleanup EXIT

echo "Fetching ANGLE frameworks from reference IPA..."
curl -fsSL -o "$TMP/ref.ipa" "$IPA_URL"
unzip -q "$TMP/ref.ipa" \
	"Payload/hl2.app/Frameworks/libEGL.framework/*" \
	"Payload/hl2.app/Frameworks/libGLESv2.framework/*" \
	-d "$TMP"

cp -R "$TMP/Payload/hl2.app/Frameworks/libEGL.framework" "$OUT/"
cp -R "$TMP/Payload/hl2.app/Frameworks/libGLESv2.framework" "$OUT/"
echo "ANGLE frameworks installed to $OUT"
