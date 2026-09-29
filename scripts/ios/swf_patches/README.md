# SWF patches

Patched copies of CS:GO movies, made from the game's own files with
`scripts/ios/swfpatch.py` (one function body swapped in place) or JPEXS FFDec
(`ffdec -export script`, edit the ActionScript, `ffdec -importScript` with just
the edited scripts; verified by exporting again and diffing). Copy them over the originals in
`Documents/csgo/resource/flash/` (keep the originals as a backup).

- `friendslisterpanel.swf`: the Friends tab's empty-state button (the tab is
  the LAN game list) opens the camera to join by QR code instead of Play:

      swfpatch.py friendslisterpanel.swf out.swf OpenOnline \
          "_global.CScaleformComponent_SteamOverlay.OpenURL('csgoios://scan')"

- `inventorypanelmaster.swf`: the Market tab becomes the Item Giver tab
  (shown only with `-unlockitemgivermenu`); selecting it opens the inventory
  panel with `CScaleformComponent_Inventory.SetItemGiverMode(true)`.
- `inventorypanel.swf`: in Item Giver mode an item's menu is Inspect and
  Receive (`CScaleformComponent_Inventory.ReceiveItem`).
