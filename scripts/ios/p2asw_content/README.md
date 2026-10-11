Files from P2ASW's `game/portal2asw/pak01_dir` (https://github.com/EpicSentry/P2ASW),
which its game code expects on top of retail Portal 2: the main menu layout
(`mainmenu_new.res`, which retail lacks) and P2ASW's versions of the options
and HUD layouts. `createipa.sh` packs them into `p2asw_dir.vpk`, which is mounted
ahead of the game's own VPKs.
