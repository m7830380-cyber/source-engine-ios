# Portal 2 iOS — round-2 adversarial analysis (do it again)

Evidence: `launch_log (117)`–`(134)`. Branch `portal2-rubberwar-ios`.
**Not committed / not CI until you greenlight.**

## Round-1 mistake caught in round-2

Round-1 almost shipped `DIFFUSE=0 VCOL=0 → packed 0 → id 0`.
**Log 125/126 already proved that combo is SKIPPED:**
```
invalid pixel shader combo
vertexlit_and_unlit_generic_ps20b static: 0 dynamic: 0
DIFFUSELIGHTING=0 VERTEXCOLOR=0 (all zero)
```
That would have been another raw unreasoned IPA. Killed.

## Ship plan after round-2

| Who | VS | PS | Notes |
|-----|----|----|-------|
| **VL** | FLATTEN → id48 | DIFFUSE=0 + SELFILLUM=1 → **1024→id170** | `g_flSelfIllumScale=0` ⇒ selfillum lerp no-op; packing only |
| **Unlit/touch** | VCOL+FLATTEN → id48 | DIFFUSE=1 → **id21** | restore VCOL (134 regression) |
| **LM** | id0 | id0 | `flLScale=1.0` unchanged this ship |

On load, log now **probes** HIT/MISS for ids `0,1,5,10,21,170,171,341,…`. Abort narrative if `170 MISS`.

---

## Debate #2 (attack every claim again)

### B2 — “Props black because DIFFUSE=1 × i.color=0” (re-attack)
**Attack:** Maybe basetexture bind fails → black; or `g_DiffuseModulation` black; or `g_fVertexAlpha` × `i.color.a=0` zeros output.
**Defense:**
- For VL, `bHasVertexAlpha` is false ⇒ `g_fVertexAlpha=0` ⇒ alpha lerp does **not** use `i.color.a` (helper ~2236). Alpha-kill hypothesis **dies**.
- Modulation multiplies *after* lighting; black×mod still black, but white lighting × mod would show tinted albedo. Ambient/id49 thrash never moved props while PS stayed id21 ⇒ failure is upstream of modulation, in `i.color`.
- Log 131/132 changed prop *appearance* (gone / stretched) when PS static changed ⇒ texture sampling path is alive; not a universal missing-VTF black.
**Ruling:** Claim B **still holds** ≥95%.

### E2 — “1024→id170 is SELFILLUM and safe” (re-attack)
**Attack:** Documents `dyn=6` vs CS:GO `.inc` dyn product 32 — packing might diverge; id170 never seen in a log dump (only first 24 ids printed).
**Defense (empirical bit→symptom alignment):**
| CS:GO packed | /6 id | Device symptom | Matches bit? |
|--------------|-------|----------------|--------------|
| 0 | 0 | invalid skipped (125) | all-zero |
| 32 | 5 | UV stretch (132) | DETAIL |
| 64 | 10 | NaN invisible (131) | CUBEMAP |
| 128 | 21 | lit×color, UVs OK (124–134) | DIFFUSE |
So `/vcsDyn` tracking of low static bits is **not luck** for DETAIL/CUBEMAP/DIFFUSE. SELFILLUM weight 1024 → 170 is the same ladder. Portal 2 uses `$selfillum` heavily ⇒ combo likely present among 8017 records.
**Residual:** If `170 MISS` or all dyn INVALID → fallback `{171,169,1,4,11}`; if those are wrong binaries, props stay wrong — **probe line is the go/no-go**.
**New mitigation:** force `g_flSelfIllumScale=0` so even a real SELFILLUM binary does not replace lighting; bit is **packing-only**. Opaque albedo×1 preserved.
**Ruling:** Best survivor. Confidence id170 correct semantics ~80–85%; confidence “better than id21 black / id0 illegal / id10/5 known bad” ≥95%.

### F2 — “Touch black squares = id49 stole VCOL from Unlit” (re-attack)
**Attack:** Texture atlas failure; ClearBuffers black rects.
**Defense:** 134 scissor blacks are ~8×48 px ticks, not button quads. Touch draws textured meshes. 133 id48 OK → 134 id49 broken with no touch.cfg delta. Unlit shares helper pins.
**Ruling:** Holds ≥95%. Gate VS VCOL on `!bVertexLitGeneric`; Unlit PS stays id21.

### Shared-PS without split? (new)
**Could both use DIFFUSE=0 SELFILLUM=1 scale0?** Unlit buttons need vertex **alpha** (and often RGB tint). VS VCOL still writes `o.color`; PS reads `i.color.a` via `g_fVertexAlpha` even if DIFFUSE=0. RGB would be fullbright texture (ignore vcol RGB) — usually OK for white icons, worse for colored ones.
**Ruling:** Keep split (Unlit DIFFUSE=1). Lower risk for touch regression un-fix.

### Nonzero DIFFUSE=0 pack table (exhaustive low bits)

| Pack bit | id≈ | Status |
|----------|-----|--------|
| (none) packed 0 | 0 | **ILLEGAL** dyn0 (125/126) |
| DETAIL | 5 | **BAD** UV (132) |
| CUBEMAP | 10 | **BAD** NaN (131) |
| ENVMAPMASK | 42 | skipped w/o cubemap |
| BASEALPHAENVMAP | 85 | needs cubemap |
| **SELFILLUM** | **170** | **survivor** (+ scale0) |
| VCOL | 341 | preferred MISS→fb id1 (125); VL would still ×black if DIFFUSE reads color |
| FLASHLIGHT | 682 | flashlight replace path |
| DECAL_BLEND=2 (+VCOL) | 41945088 | **ILLEGAL** (121) |

### Walls
`flLScale=1.0` stays. No thrash in this ship. Revisit only after props/touch IPA.

---

## Hard stop

- packed 0 / id 0 for VL PS  
- CUBEMAP/DETAIL unlit  
- id49 VL VS without new proof  
- flLScale random shrink with VL  
- Ungated Unlit pins  

## Pass criteria (log 135)

1. `probe staticId=170 HIT` (else stop and re-plan)  
2. `ps20b: packed 1024 → id 170` (VL)  
3. `ps20b: packed 128 → id 21` (Unlit)  
4. `vs20: … → id 48`  
5. No `invalid pixel shader combo` for VL static 0  
6. Touch icons visible; props show albedo (likely fullbright)  
