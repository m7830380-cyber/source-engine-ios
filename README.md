# CS:GO (cstrike15) for iOS — work in progress

This branch ports CS:GO's own engine and game code (`cstrike15_src`) to iOS
arm64, reusing the iOS platform work from
[nillerusr/source-engine](https://github.com/nillerusr/source-engine): the waf
build, SDL2, ANGLE (GLES over Metal), the GLES `togl` backend and IPA
packaging.

Everything compiles and links, and CI produces an unsigned IPA. It has **not
been run on a device yet**, and there is no menu/HUD UI (see Status).

## How the build works

- Valve's `.vpc` project scripts stay the source of truth.
  `scripts/waifulib/vpc.py` reads them (`$Include`, `$Macro`, `$Conditional`,
  `[conditions]` including per-file ones, `-$File`, ...) and `wscript` turns
  every project into a waf task generator, following `$Lib`/`$ImpLib` links
  from the runtime modules in `ROOT_PROJECTS`.
- iOS is built as CS:GO's `OSX64` configuration (`POSIX`, `OSX`, `GL`, `SDL`)
  plus `IOS=1`. Where macOS code does not apply (AppKit, Carbon, IOKit, CoreAudio
  HAL), iOS takes CS:GO's Linux/SDL code paths instead. As on Valve's Mac
  build, `materialsystem` links the shader API and shaders in statically.
- `togl` is the source-engine port's GLES D3D9->GL layer (same generation as
  CS:GO's desktop `togl`), with CS:GO's type sizes and API defaults.
- `ios/include` holds compatibility headers: `<malloc.h>`, x86 SSE intrinsics
  mapped to NEON through sse2neon, and a Carbon umbrella with the
  CoreFoundation/CoreGraphics parts iOS has.
- `.proto` files are compiled with protoc 2.5.0 built from
  `thirdparty/protobuf-2.5.0` (the version CS:GO pins); squirrel `.nut`
  scripts are embedded at build time.
- Libraries that are not in the source tree are replaced:
  - Scaleform (menus, HUD): `scaleformui/null`, a do-nothing `IScaleformUI`
    generated from the interface by `scripts/gen_null_scaleform.py`.
  - Steam: `stub_steam`, which reports Steam as not running.
  - Game coordinator SDK: `gcsdk/gcsdk_ios.cpp`, the shared object and
    protobuf message parts the item system uses locally.
  - Steam Datagram Relay and Steam Audio: engine stubs in `ios/engine`.
  - fontconfig: `ios/fontconfig`, font lookup by scanning font directories.
  - Physics: the port's open `vphysics` + `ivp`, extended to CS:GO's
    physics interface.
- Crypto++ 5.6.1 is built from `external/crypto++-5.61`.
- iOS dependencies (zlib, libpng, libjpeg, freetype, curl, protobuf, SDL2,
  ANGLE) are built by `scripts/ios/*.sh` from the `ios/thirdparty` submodule.

## Building

CI (`.github/workflows/build-ios-ipa.yml`) runs `scripts/build-ios-arm64.sh`
on a macOS runner and uploads the unsigned IPA
(`source-engine-csgo-ios-arm64`). Locally on a Mac:

```sh
git submodule update --init --recursive
./scripts/build-ios-arm64.sh
```

Set `WAF_PROJECTS=tier0,tier1` to build only some projects and their
dependencies.

## Installing and game data

The IPA is unsigned; sign it with your own tooling (AltStore, Sideloadly, a
developer certificate, ...).

The app needs the `csgo/` game data (VPKs, maps, shaders) from a CS:GO
install; it is not part of this repository. Copy the `csgo` folder into the
app's Documents directory (Finder file sharing or the Files app). On launch a
dialog lets you edit the command line (default `-game csgo -dev 2 -log`).

## Status

- [x] VPC-driven waf build, protobuf 2.5.0, Crypto++
- [x] All modules compile for iOS arm64
- [x] Link and package (unsigned IPA from CI)
- [x] GLES backend for togl
- [ ] First run on a device: boot to the console / load a map
- [ ] Touch controls
- [ ] Replacement for the Scaleform UI (menus, HUD)
