# Portal 2 iOS — shader/UI freeze (do not thrash)

Evidence: device `launch_log (117)`–`(132)` under Telegram Desktop dumps.
Branch: `portal2-rubberwar-ios`.

## Hard lesson (log 130–132)

Do **not** chase black props by swapping VL static pins (CUBEMAP id10, DETAIL id5).
Those broke UVs / made props invisible. Black props were **ambient + dyn bind**, not the wrong static id.

| Log | VL remap | Symptom | Real cause |
|-----|----------|---------|------------|
| **124** | VS `9216→48`, PS `128→21` | Best baseline | — |
| 130 | same 21 | Props **black**, walls bright | Empty ambient cube; “force white” was a comment only |
| 131 | PS `64→10` CUBEMAP | Props **invisible** | PS cubemap + zero VS normals → NaN |
| 132 | PS `32→5` DETAIL | Stretched UVs + fullbright look | PS/VS interpolator mismatch |

## Frozen pins (match log 124)

- **Remapper:** `staticId = packed / vcsHeader.m_nDynamicCombos` only. Never `.inc` dyn first.
- **VL PS:** `DIFFUSELIGHTING=1`, `VERTEXCOLOR=0`, `CUBEMAP=0`, `DETAILTEXTURE=0` → packed **128** → id **21**.
- **VL VS:** `FLATTEN_STATIC_CONTROL_FLOW=1`, `VERTEXCOLOR=0` → packed **9216** → id **48**.
- **VL lighting (real fix, not a pin):**
  - Keep `DYNAMIC_LIGHT=1`. Dyn fallback must **prefer slots with bit1 set** (DYNAMIC_LIGHT), not dyn0.
  - `CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE`: if lighting state NULL or luminance &lt; 0.05, write fill cube **0.45**.
- **LM PS/VS:** static id 0, `FASTPATH=0`. Lightmap bind without SRGBREAD (format mismatch). **No** flLScale thrash.
- **VGUI solids:** ClearBuffers. In-game touch: corner ticks only.
- **Fonts:** Helvetica `font=63`.

## Hard stop — do not change without a new named log + root-cause writeup

- CUBEMAP=1 / DETAILTEXTURE=1 / DIFFUSE=0 “unlit albedo” experiments on VL PS
- Flip LM FASTPATH to 1
- VL VS FLATTEN=0
- Prefer `.inc` dyn in `ResolveStaticComboRecordIndex`
- Nest `SET_STATIC_*` in extra `{ }` (CI undeclared forgot_to_set)
- Blind `flLScale *= k` without proving lightmap sample path

## Next real work

1. Device-verify: remap `128→21`, props textured (not stretched), not black.
2. If walls still washed: fix lightmap sRGB at **texture create** time, not scale hacks.
3. Matching Documents `.vcs` with real VERTEXCOLOR static for mesh UI.
