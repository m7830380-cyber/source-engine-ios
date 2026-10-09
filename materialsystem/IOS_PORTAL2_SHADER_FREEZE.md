# Portal 2 iOS — log 135 autopsy (no more guessing)

## What log 135 proved (≥99%)

```
probe staticId=170 MISS
probe staticId=171 MISS
probe staticId=341 MISS
probe staticId=682 MISS
… HIT: 0,1,5,10,21,42,85
vertexlit_and_unlit_generic_ps20b: static packed 1024 → fallback staticId 1
```

| Intent | Actual | Result |
|--------|--------|--------|
| VL PS SELFILLUM→id170 | **170 does not exist** | fallback **id 1** |
| id 1 = “unlit” | **props still black** | id1 behaves like DIFFUSE×black (or equivalent) |
| Unlit VS VCOL | packed 9360→id48 | touch path OK (not the complaint) |
| Walls flLScale=1.0 | brightness better | **no CSM / no flashlight shadows** (still forced off) |

Round-2 pass bar said: if `170 MISS`, stop. We shipped anyway. That was the failure.

## Why props stay black (mechanism unchanged)

PS `DIFFUSE=1` ⇒ `diffuseLighting = i.color`. VS id48 VCOL ⇒ prop color stream 0 ⇒ black.
id49 lighting + white ambient still black (134). So we **must** use a PS with `DIFFUSE=0` that **exists**.

## Existing DIFFUSE=0 statics (device evidence)

| id | packed | Log | Visual |
|----|--------|-----|--------|
| 0 | 0 | 125/126 | **INVALID** dyn0 |
| 5 | 32 DETAIL | **132** | **VISIBLE** but UV stretch |
| 10 | 64 CUBEMAP | 131 | invisible NaN |
| 170 | 1024 SELFILLUM | **135** | **MISS** |

Only **id 5** is a known-good DIFFUSE=0 binary that draws.

## Why 132 stretched (revised)

`DETAIL_BLEND_MODE=0` = `base *= lerp(1, 2*detail, factor)`.
Default **factor=1** + unbound/garbage detail ⇒ looks like stretched/wrong albedo.
**factor=0** ⇒ `base *= 1` (identity). Detail sample irrelevant.

## Next pin (in tree, not CI until you say)

- VL PS: `DETAIL=1 DIFFUSE=0` → packed **32 → id 5** (HIT)
- Force `g_DetailBlendFactor=0`, bind `TEXTURE_WHITE` on sampler 2
- Unlit unchanged: DIFFUSE id21 + VS VCOL
- Fallback list: prefer **5**, never **1** or **0** or **10**

Expected log: `packed 32 → id 5`, props textured. If UVs still stretch with factor 0 ⇒ interpolator mismatch (then separate fight). If props visible with good UVs ⇒ win.

## Walls / shadows (separate)

`flLScale=1.0` fixed overbright wash. Shadows still gone because iOS pins **CSM=0**, flashlight path disabled, FASTPATH=0. Baked lightmap darkening should still appear if LM samples; user “no shadows” may mean dynamic/CSM. Do **not** couple shadow work into the VL pin ship.

## Hard stop

- Betting on MISS static ids (170/341/682)
- Falling back to id **1** for “unlit”
- packed 0 / id 0
- CUBEMAP id10
- Blind flLScale thrash with VL
