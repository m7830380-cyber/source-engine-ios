# Portal 2 iOS — shader/UI freeze (do not thrash)

Evidence: device `launch_log (117)`–`(132)` under Telegram Desktop dumps.
Branch: `portal2-rubberwar-ios`.

## Hard lesson (log 130–133)

| Log | Change | Symptom | Actual root cause |
|-----|--------|---------|-------------------|
| 131–132 | PS CUBEMAP/DETAIL unlit | invisible / stretched UV | PS/VS combo mismatch — **do not** |
| 133 | restore PS id21 + ambient fill | props **still black**, walls fullbright | See below |

### Props black (log 133) — real cause
- iOS forces `bHasBump=false` → non-bump VL.
- White VS ambient was **already** forced in helper — still black ⇒ `AmbientLight()` never runs.
- VS `FLATTEN` packed 9216 → id **48**; `VERTEXCOLOR` +144 → 9360 → **same id 48** (dyn=192 alias).
- Documents id 48 is the **VCOL** binary: skips DoLighting, reads color stream.
- VertexLitGeneric snapshot does **not** enable `VERTEX_COLOR` → attribute = 0 → black.
- Fix: VS `CUBEMAP+FLATTEN` packed **9504 → id 49** (VCOL cannot live in that bucket).

### Walls fullbright — real cause
- `GetLightMapScaleFactor()` = `GammaToLinearFullRange(2)` ≈ **4.59** (PC overbright for **sRGB-decoded** lightmaps).
- iOS binds lightmap as raw RGBA8 (no SRGBREAD).
- 4.59 × gamma samples → blowout. Fix: **`flLScale = 1.0`** on iOS (not a random shrink).

## Frozen pins (log 133+)

- **VL PS:** DIFFUSE=1 → packed 128 → id **21** (UVs OK).
- **VL VS:** CUBEMAP=1 + FLATTEN=1 → packed **9504** → id **49**. Not id 48.
- **LM:** static 0, FASTPATH=0, no SRGBREAD bind, **`flLScale = 1.0`** on iOS.
- Keep DYNAMIC_LIGHT=1 + ambient fill as belt-and-suspenders.
- VGUI: ClearBuffers. Fonts: Helvetica.

## Hard stop

- PS CUBEMAP/DETAIL “unlit” experiments
- Blind `flLScale *= 0.35/0.5`
- VL VS back to id 48 without proving Documents VCOL alias is gone
- Nest `SET_STATIC_*` in extra `{ }`
