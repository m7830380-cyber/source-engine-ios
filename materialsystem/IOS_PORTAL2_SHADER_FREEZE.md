# Portal 2 iOS — stop PS static-id roulette

## Root cause (log 135, not another PS remap)

```
vertexlit_and_unlit_generic_vs20: static packed 9360 → id 48
vertexlit_and_unlit_generic_ps20b: static packed 128 → id 21 (DIFFUSE=1)
```

Black props = **wrong VS bytecode at id 48** (Documents VCS **VCOL alias**), not missing PS combo.

- PS `DIFFUSE=1` multiplies albedo by `i.color`.
- id **48** bytecode behaves like **Unlit VERTEXCOLOR** → `i.color` ≈ 0 on props.
- White ambient + `DYNAMIC_LIGHT=1` did not help while id48 was bound.

`VERTEXCOLOR` adds **16** to packed static index. After `/192` both VL and Unlit collapse to **id 48**.

Discriminant (before divide): **`packed % 32 == 16`** ⇒ VL lit pin; **`== 0`** ⇒ Unlit VCOL (touch).

## Fix in tree (no id5 / id170 / DETAIL hack)

1. **VS resolve**: when packed is VL lit and lookup hits id 48, bind **id 49** (then 50, 47…) — log line: `skip VCOL alias id 48`.
2. **VS pin**: `FLATTEN_STATIC_CONTROL_FLOW = 0` for `bVertexLitGeneric` only (Unlit keeps 1).
3. **PS pin**: restore **`DIFFUSE=1` → packed 128 → id 21** (correct UVs + lighting).
4. **Ambient fill**: when instance lighting is empty, force **1.0** cube (was 0.45).

## Pass bar (next log)

| Check | Pass |
|-------|------|
| VS | `packed 9360` → **id 49** (or lit remap msg), **not** bare `→ id 48` |
| PS | `packed 128 → id 21` |
| Props | Textured, not black |
| Touch | Still uses id 48 path (`packed % 32 == 0`) |

## If still black

Do **not** loop PS static ids. Next: TOGL varying / `SetVertexShaderStateAmbientLightCube` register layout, or ship corrected `.vcs` for vs20.

## Walls / shadows (unchanged)

`flLScale=1.0` only. CSM/flashlight still off on iOS pins.
