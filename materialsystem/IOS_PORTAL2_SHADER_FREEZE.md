# Portal 2 iOS — vertexlit combo map (bundled `fxc` .vcs)

Engine pins (iOS) must match **enumerated static ids** from `vcscompile.py`, not Documents retail `packed / dyn`.

## Authoritative static ids (vertexlit_and_unlit_generic_ps20b, tree)

| Material flags (iOS pins) | Packed `GetIndex()` | **fxc static sid** | Notes |
|---------------------------|---------------------|--------------------|--------|
| `DIFFUSELIGHTING=1`, rest 0 | 128 | **4** | Props; multiply by VS lighting |
| `VERTEXCOLOR=0`, unlit rest 0 | 0 | **0** | Fonts / touch (R21 IOS PS fastpath + c1); **not sid 64** if aliased/wrong |
| `DIFFUSE=0`, `VCOL=0` | 0 | 0 | **SKIP** — invalid combo (log 125 white world) |

VS (tree, dyn=144): props `9216` → **64**; unlit UI `9360` → **65**.

## CI requirements

`build-portal2-ipa.yml` must compile and verify **PS static ids 0 and 4** in `vertexlit_and_unlit_generic_ps20b.vcs`.

Check:

```
probe staticId=0 HIT
[iOS] Shader 'vertexlit_and_unlit_generic_ps20b': static packed 0 → tree PS staticId 0 (dyn=32).
```

If `staticId=64 MISS` or log shows `→ staticId 1` for 2048, the IPA shader blob is wrong — fix CI, not C++ remaps.

## Runtime (R7)

- Remap: packed **0** → sid **0** for unlit UI (R21); props 128 → **4**.
- VGUI: mesh COLOR0 RGB forced white; tint via `IMaterial::ColorModulate` → PS c1.
- Props: reject near-black compiled ambient cubes; min cube 1.0; per-draw white ambient + PS c1.

## Do not

- Map 2048 → staticId **1** (DETAIL) or **42** (CUBEMAP+SELFILLUM, not VCOL-only).
- Map 128 → staticId **21** unless that sid exists in the **bundled** file (usually it does not).
- Force `DIFFUSE=0` + `VCOL=0` (skipped PS combo).
