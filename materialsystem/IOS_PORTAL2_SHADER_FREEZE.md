# Portal 2 iOS — log 136 autopsy

## What broke (136 vs “repeat” builds)

```
!!!!!Using invalid shader combo!!!!!
vertexlit_and_unlit_generic_vs20 static: 0 dynamic: 3
static: … FLATTEN_STATIC_CONTROL_FLOW=0 VERTEXCOLOR=0 …
dynamic: COMPRESSED_VERTS=1 DYNAMIC_LIGHT=1 … NUM_LIGHTS=0
```

| Bug | Effect |
|-----|--------|
| **FLATTEN=0 on VL** | All other iOS static pins are 0 → **packed static 0 → id 0** (not 9360/49) |
| **dyn = 3** | COMPRESSED_VERTS(1) + DYNAMIC_LIGHT(2) = **skipped slot** on that static |
| **PS VERTEXCOLOR=0 for Unlit** | Touch/VGUI lost vertex-color path → **transparent black squares** |

First-load line `9360 → id 49` is only the **one-time** VCS spew; **in-game props used static id 0** per invalid-combo dump.

## Fix in tree

1. **FLATTEN=1** again (restores nonzero packed / id 49 remap path).
2. **VS dyn: COMPRESSED_VERTS=0** on iOS (dyn **2** = DYNAMIC_LIGHT only).
3. **PS gate**: VL `DIFFUSE=1` + `VERTEXCOLOR=0`; Unlit `DIFFUSE=0` + `VERTEXCOLOR=1` (touch).
4. Keep **skip id48 → id49** for VL lit (`packed % 32 == 16`).

## Pass bar (log 137)

- **No** `invalid shader combo` for `vertexlit_and_unlit_generic_vs20`
- VS `packed 9360 → id 49` (or skip-48 msg); **not** props drawing at `static: 0`
- Unlit/touch: VS packed with **VERTEXCOLOR** (e.g. 9376) → **id 48**; PS **VERTEXCOLOR=1**
- Props: not black (if still black with **valid** dyn2/id49 → lighting path, not illegal combo)

## Do not

- FLATTEN=0 on VL (collapses packed to 0).
- PS `VERTEXCOLOR=0` on Unlit (kills touch).
