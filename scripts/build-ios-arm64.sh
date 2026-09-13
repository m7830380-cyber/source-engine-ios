#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

if ! command -v autoconf >/dev/null; then
	brew install autoconf automake libtool pkg-config
fi

git submodule sync --recursive
git submodule update --init --recursive

chmod +x scripts/ios/*.sh waf

scripts/ios/build-deps.sh
scripts/ios/build-sdl2.sh
scripts/ios/build-angle.sh

export ANGLE_FRAMEWORK_PATH="$ROOT/build/ios"
SDL2_FW="$ROOT/build/ios/SDL2.framework"

./waf configure -T release --disable-warns --ios --angle --togles \
	--sdl2="$SDL2_FW" \
	"$@"

./waf build
scripts/ios/createipa.sh

mkdir -p artifacts
cp build/ios/source-engine.ipa artifacts/source-engine-ios-arm64.ipa
