#!/bin/bash
# Fetch P2ASW's game content (EpicSentry/P2ASW game/portal2asw) for the IPA:
# its sound scripts, map scripts, scenes, menu/HUD layouts, localization and
# models. Its converted voice lines (sound/vo_converted) are left out: the
# player's own Portal 2 install supplies sound/vo. createipa.sh
# packs the result into p2asw_dir.vpk, mounted ahead of the game's own VPKs,
# like P2ASW's folder heads its gameinfo search paths.
#
#   scripts/ios/fetch_p2asw_content.sh [out_dir]   (default build/ios/p2asw_content)
set -euo pipefail

P2ASW_COMMIT="${P2ASW_COMMIT:-0df06fbfa462728de28d43aab4c0867572952840}"
cd "${0%/*}/../.."
OUT="${1:-build/ios/p2asw_content}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

curl -sfL "https://codeload.github.com/EpicSentry/P2ASW/tar.gz/$P2ASW_COMMIT" |
	tar -xz -C "$TMP" "P2ASW-$P2ASW_COMMIT/game/portal2asw"
SRC="$TMP/P2ASW-$P2ASW_COMMIT/game/portal2asw"

# Windows binaries, shaders and caches built for the Alien Swarm engine, the
# mod's own gameinfo/pak pair (pak01_dir/ holds the same files loose), editor files
rm -rf "$SRC/bin" "$SRC/shaders" "$SRC/maps/soundcache" "$SRC/modelsounds.cache" \
	"$SRC/gameinfo.txt" "$SRC/steam.inf" "$SRC/run_server.bat" "$SRC"/*.fgd "$SRC"/pak01_*.vpk
# P2ASW's pak01 overrides its loose files
cp -R "$SRC/pak01_dir/." "$SRC/"
rm -rf "$SRC/pak01_dir"

# Voice lines come from the player's install, not P2ASW's PCM conversions
# (Alien Swarm can't play Portal 2's MP3-in-.wav VO; this engine can).
rm -rf "$SRC/sound/vo_converted"
grep -rlZ "vo_converted" "$SRC/scripts" --include='*.txt' | xargs -0 -r sed -i.bak 's#vo_converted/#vo/#g'
find "$SRC/scripts" -name '*.bak' -delete
if grep -rq "vo_converted" "$SRC/scripts"; then
	echo "P2ASW scripts still reference vo_converted" >&2
	exit 1
fi

rm -rf "$OUT"
mkdir -p "$(dirname "$OUT")"
mv "$SRC" "$OUT"
echo "P2ASW content ($P2ASW_COMMIT): $(find "$OUT" -type f | wc -l | tr -d ' ') files, $(du -sh "$OUT" | cut -f1)"
