# Portal 2 iOS — log 137 autopsy

## Why 137 looked identical to 136 visually

| Line | Meaning |
|------|---------|
| `9360 → id 49` | Same VL VS as 136 (skip-48 remap) |
| `packed 128 → id 21` | Props still on **DIFFUSE PS id21** (× black `i.color`) |
| `packed 2048 → fallback id 21` | **Touch killer** — Unlit VCOL packed 1024/2048 must not bind id21 |
| No `invalid shader combo` | 3819f196 fix worked; **wrong shaders**, not illegal dyn3 |

## Fallback bug (fixed in tree)

`IOS_FallbackStaticComboRecordIndex` used `(packed & 128)` for “DIFFUSE”. On ps20b **128 = ENVMAPMASK**, **DIFFUSE = 64**, **VERTEXCOLOR = 1024**.

Unlit touch (VCOL=1) → packed **1024** or **2048** → fell into “unlit” list starting with **id 21** (DIFFUSE lit).

## In-tree changes

1. PS fallback: **1024 bit → vcol ids (42, 85, …)**; **64 → diffuse**; **16 → detail id5**.
2. VL PS: **SFM+DETAIL → packed 32 → id 5**, DIFFUSE=0, detail blend **0**, white sampler.
3. Unlit PS: **VERTEXCOLOR=1**, DIFFUSE=0 (unchanged intent).
4. Keep: FLATTEN=1, COMPRESSED_VERTS=0, VS skip id48→49.

## Pass bar (log 138)

- `packed 2048` or `1024` → **not** `fallback staticId 21`
- VL PS: `packed 32 → id 5` (or direct hit)
- Touch visible; props textured (may be flat-lit until real VS lighting)
