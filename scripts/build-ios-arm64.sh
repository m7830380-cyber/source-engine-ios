#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

if ! command -v autoconf >/dev/null; then
	brew install autoconf automake libtool pkg-config
fi

git submodule sync --recursive
git submodule update --init --recursive

# iOS/Apple: IVP includes <malloc.h> when OSX is not defined; use Darwin path.
if [ -f ivp/ivp_utility/ivu_types.hxx ]; then
	python3 - <<'PY'
from pathlib import Path
p = Path("ivp/ivp_utility/ivu_types.hxx")
text = p.read_text()
old = """#if !defined(__MWERKS__) || !defined(__POWERPC__)
#   ifdef OSX
#       include <malloc/malloc.h>
#   else
#       include <malloc.h>
#   endif
#endif"""
new = """#if !defined(__MWERKS__) || !defined(__POWERPC__)
#   if defined(OSX) || defined(IOS) || defined(_IOS) || defined(__APPLE__)
#       include <malloc/malloc.h>
#   else
#       include <malloc.h>
#   endif
#endif"""
if old in text:
	p.write_text(text.replace(old, new, 1))
	print("Patched ivp malloc include for Apple/iOS")
elif "malloc/malloc.h" in text and "__APPLE__" in text:
	print("IVP malloc include already patched")
else:
	print("WARNING: unexpected ivu_types.hxx malloc include block")
PY
fi

chmod +x scripts/ios/*.sh waf

scripts/ios/build-deps.sh
scripts/ios/build-sdl2.sh
scripts/ios/build-angle.sh

export ANGLE_FRAMEWORK_PATH="$ROOT/build/ios"
SDL2_FW="$ROOT/build/ios/SDL2.framework"
export CFLAGS="-I${SDL2_FW}/Headers ${CFLAGS:-}"
export CXXFLAGS="-I${SDL2_FW}/Headers ${CXXFLAGS:-}"

./waf configure -T release --disable-warns --ios --angle --togles \
	--skip-sdl2-sanity-check \
	--sdl2="$SDL2_FW" \
	"$@"

./waf build
scripts/ios/createipa.sh

mkdir -p artifacts
cp build/ios/source-engine.ipa artifacts/source-engine-ios-arm64.ipa
