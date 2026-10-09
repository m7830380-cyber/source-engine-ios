# Portal 2 iOS — shader/UI freeze (do not thrash)

Evidence: device `launch_log (117)`–`(127)` under Telegram Desktop dumps.
Branch: `portal2-rubberwar-ios` (linear +127 from `portal2-ios`).

## Best device evidence

| Rank | Log | Why |
|------|-----|-----|
| 1 | **124** | VL `9216→48` / PS `128→21`, LM `0→0`, Helvetica `font=63`, **no** invalid combo, signon 6 |
| 2 | **117** | Pre-remap clean playable room, `fullbright=0`, cyan menu, `font=0` |
| worst | **127** | Cyan-only menu (mesh fills), never stable signon 6 |

Do **not** optimize against 125–126 (VERTEXCOLOR→fallback id 1, black/cyan scissor floods).

## Frozen pins (match log 124)

- **Remapper:** `staticId = packed / vcsHeader.m_nDynamicCombos` only. Never `.inc` dyn first (log 118: 128/32→id 4 black).
- **VL PS:** `DIFFUSELIGHTING=1`, `VERTEXCOLOR=0` → packed 128 → id **21**.
- **VL VS:** `FLATTEN_STATIC_CONTROL_FLOW=1`, `VERTEXCOLOR=0` → packed 9216 → id **48**.
- **LM PS/VS:** `FASTPATH=0` (117/124 era; FASTPATH=1 correlated with white flash).
- **Ambient:** force white cube while `NUM_LIGHTS` pinned 0 (else black props).
- **VGUI solids:** `IOSDrawFilledRect` → scissor `ClearBuffers` (mesh UnlitGeneric invisible — log 127).
- **In-game touch paint:** corner ticks only (full ClearBuffers punches world — log 126).
- **Fonts:** Helvetica scheme fallback (`font=63`); PLAY block letters until matching `.vcs` gives real VERTEXCOLOR.

## Hard stop — do not change without a new named log

- Re-enable mesh `DrawFilledRect` / Unlit VERTEXCOLOR chase
- Flip LM FASTPATH to 1
- Set VL PS DIFFUSE=0 or illegal static 0
- Set VL VS FLATTEN=0 (id 0 skipped → rainbow)
- Prefer `.inc` dyn in `ResolveStaticComboRecordIndex`
- Nest `SET_STATIC_PIXEL_SHADER_COMBO` in extra `{ }` (CI undeclared forgot_to_set)

## Next real work (not pin churn)

1. Device-verify IPA from restore commits (`79987530` / `aa8ab3ca`+) against **log-124** symptoms:
   - remaps `9216→48`, `128→21`, LM `0→0`, Helvetica `font=63`, no invalid combo, signon 6.
2. **Portal product (not shaders):** restore `portalrenderable_flatbasic.cpp` + `portal_gamemovement.cpp` (rubberwar had empty stubs / VPC excludes — portals never drew linked views).
3. Matching Documents `vertexlit_and_unlit_generic_{vs20,ps20b}.vcs` so VERTEXCOLOR is a distinct static (today 9216 and 9360 both → 48).
4. **Hypothesis (do not ship blind):** mesh fills may work on id 21 if `$vertexcolor` is **off** and color comes from `$color` / texture bake — log 127 invisibility was VERTEXCOLOR→fallback id 1, not “all meshes dead”. Prove with a tiny in-game tick experiment only; keep menu on ClearBuffers until a named log confirms.
5. Real GameUI on top of a frozen shader baseline — separate track.
