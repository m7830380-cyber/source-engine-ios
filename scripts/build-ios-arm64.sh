#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

if ! command -v autoconf >/dev/null; then
	brew install autoconf automake libtool pkg-config
fi

git submodule sync --recursive
git submodule update --init --recursive

# iOS/Apple: IVP includes <malloc.h> on non-OSX Apple builds; rewrite to Darwin path.
if [ -d ivp ]; then
	python3 - <<'PY'
from pathlib import Path
root = Path("ivp")
patched = 0
for path in root.rglob("*"):
	if path.suffix.lower() not in {".h", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".hpp"}:
		continue
	text = path.read_text(errors="ignore")
	orig = text
	# Prefer Darwin malloc when OSX gate is present but iOS would miss it.
	text = text.replace(
		"#ifdef OSX\n#include <malloc/malloc.h>\n#else\n#include <malloc.h>\n#endif",
		"#if defined(OSX) || defined(IOS) || defined(_IOS) || defined(__APPLE__)\n#include <malloc/malloc.h>\n#else\n#include <malloc.h>\n#endif",
	)
	text = text.replace(
		"#   ifdef OSX\n#       include <malloc/malloc.h>\n#   else\n#       include <malloc.h>\n#   endif",
		"#   if defined(OSX) || defined(IOS) || defined(_IOS) || defined(__APPLE__)\n#       include <malloc/malloc.h>\n#   else\n#       include <malloc.h>\n#   endif",
	)
	# Bare includes (no OSX gate).
	if "#include <malloc.h>" in text and "malloc/malloc.h" not in text.split("#include <malloc.h>")[0][-80:]:
		text = text.replace("#include <malloc.h>", "#include <malloc/malloc.h>")
	if text != orig:
		path.write_text(text)
		patched += 1
		print(f"Patched {path}")
print(f"IVP malloc patches applied: {patched}")
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
