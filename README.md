# CS:GO (cstrike15) for iOS — work in progress

This branch ports CS:GO's own engine and game code (`cstrike15_src`) to iOS
arm64, reusing the iOS platform work from
[nillerusr/source-engine](https://github.com/nillerusr/source-engine): the waf
build, SDL2, ANGLE (GLES over Metal) and IPA packaging.

It does **not** run yet. Status is tracked below.

## How the build works

- Valve's `.vpc` project scripts stay the source of truth.
  `scripts/waifulib/vpc.py` reads them (`$Include`, `$Macro`, `$Conditional`,
  `[conditions]`, `-$File`, ...) and `wscript` turns every project into a waf
  task generator, following `$Lib`/`$ImpLib` links from the runtime modules in
  `ROOT_PROJECTS`.
- iOS is built as a variant of CS:GO's `OSX64` configuration (`POSIX`, `OSX`,
  `GL`, `SDL`) plus `IOS=1`. Scaleform (`INCLUDE_SCALEFORM`), Steam and CEG are
  compiled out.
- `.proto` files are compiled with protoc 2.5.0 built from
  `thirdparty/protobuf-2.5.0` (the version CS:GO pins).
- Crypto++ 5.6.1 is built from `external/crypto++-5.61`.
- Physics uses nillerusr's open `vphysics` + `ivp` (not in the CS:GO leak).
- iOS dependencies (zlib, libpng, libjpeg, freetype, curl, protobuf, SDL2,
  ANGLE) are built by `scripts/ios/*.sh` from the `ios/thirdparty` submodule.

## Building

CI (`.github/workflows/build-ios-ipa.yml`) runs `scripts/build-ios-arm64.sh`
on a macOS runner and uploads the unsigned IPA. Locally on a Mac:

```sh
git submodule update --init --recursive
./scripts/build-ios-arm64.sh
```

Set `WAF_PROJECTS=tier0,tier1` to build only some projects and their
dependencies.

## Game data

The app needs the `csgo/` game data (VPKs, maps, shaders) from a CS:GO
install; it is not part of this repository.

## Status

- [x] VPC-driven waf build, protobuf 2.5.0, Crypto++
- [ ] All modules compile for iOS arm64
- [ ] Link and package
- [ ] GLES backend for togl
- [ ] Replacement for the Scaleform UI (menus, HUD)
- [ ] Boots to a map
