# Portal 2 iOS — stop remapping; ship tree .vcs

## Log 138 (why remaps change nothing)

- Still loading **Documents/platform** CS:GO `.vcs` (`dyn=192` vs20, `dyn=6` ps).
- `packed 2048 → fallback staticId 4` / `packed 64 → id 10` — retail aliases, not engine `fxctmp9/*.inc` combos.
- `9360 → id 49` remap does not fix bytecode: id49 in retail ≠ combo the C++ requests.

**Root cause:** engine C++ and `fxctmp9/*.inc` are one combo space; Documents retail `.vcs` is another. Runtime id5/id21/id49 roulette cannot fix that.

## Fix (in tree)

1. **`vcscompile.py`** compiles `.fxc` → `.vcs` with the same combo math as `.inc` (see `scripts/shaders/vcscompile.py` header).
2. **IPA CI** (`build-portal2-ipa.yml`): `compile-shaders` job (Windows) → bundle `platform/shaders/fxc/*.vcs` into `.app`.
3. **Launcher**: `APP_LIB_PATH/platform` on **PLATFORM** search path **before** Documents `platform`.
4. **Removed** iOS static combo pins in `vertexlitgeneric_dx9_helper.cpp` (stock material combos again).

Keep (for now): iOS dynamic pins (`DYNAMIC_LIGHT`, `COMPRESSED_VERTS=0`), ambient fill, lightmap scale — not static-id hacks.

## Pass bar (log 139+)

```
[iOS] VCS vertexlit_and_unlit_generic_ps20b.vcs: ver=6 dyn=<matches inc, NOT retail 6 with wrong bytecode>
```

- No `fallback staticId` for normal prop/touch draws if combo exists in bundled file.
- `Bundled N PLATFORM shader(s)` in CI packaging log.
- Props textured; touch not black squares.

## If still broken

Run `python scripts/shaders/vcscompile.py count vertexlit_and_unlit_generic_ps2x.fxc vertexlit_and_unlit_generic_ps20b` and add missing static ids to CI `--static-ids` list — do **not** add another runtime remap.
