# SWF patches

Patched copies of CS:GO movies, made from the game's own files with
`scripts/ios/swfpatch.py`. Copy them over the originals in
`Documents/csgo/resource/flash/` (keep the originals as a backup).

- `friendslisterpanel.swf`: the Friends tab's empty-state button (the tab is
  the LAN game list) opens the camera to join by QR code instead of Play:

      swfpatch.py friendslisterpanel.swf out.swf OpenOnline \
          "_global.CScaleformComponent_SteamOverlay.OpenURL('csgoios://scan')"
