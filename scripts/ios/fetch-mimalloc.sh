#!/bin/bash
# Fetch mimalloc for the optional --mimalloc tier0 allocator backend.
# No-op if it is already present, so CI re-runs stay cheap.
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"

MIMALLOC_VERSION="v2.1.7"
DEST="$ROOT/thirdparty/mimalloc"

if [ -d "$DEST/include" ]; then
	echo "mimalloc already present at $DEST, skipping fetch."
	exit 0
fi

echo "Fetching mimalloc $MIMALLOC_VERSION ..."
mkdir -p "$ROOT/thirdparty"
git clone --depth 1 --branch "$MIMALLOC_VERSION" \
	https://github.com/microsoft/mimalloc.git "$DEST"

# Sanity check: the amalgamated TU we compile into tier0 must exist.
if [ ! -f "$DEST/src/static.c" ]; then
	echo "ERROR: mimalloc checkout is missing src/static.c" >&2
	exit 1
fi

echo "mimalloc ready at $DEST"
