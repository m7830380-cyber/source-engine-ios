#!/usr/bin/env python3
"""Guard against the Valve/Objective-C BOOL collision.

public/tier0/basetypes.h has `typedef int BOOL`.
Objective-C's <objc/objc.h> has `typedef bool BOOL`.

Any translation unit that pulls in both fails with:
    error: typedef redefinition with different types ('bool' vs 'int')

The repo already hit this once (commit 14f6ad3d, "Fix iOS CI: keep UIKit
BOOL out of glmrendererinfo") and the fix is structural: a .mm file that
includes ObjC framework headers must not include Valve headers, and talks
to the C++ side through a plain C bridge header instead.

This script enforces that rule so the mistake cannot silently come back.
Run it before pushing; CI cannot catch it any faster than a 10 minute
build, and this takes under a second.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Headers that (transitively) define Valve's BOOL.
VALVE_HEADERS = re.compile(
    r'#\s*include\s*[<"]('
    r'tier0/|tier1/|tier2/|tier3/|'
    r'audio_pch\.h|basetypes\.h|platform\.h|dbg\.h|'
    r'convar\.h|utlvector\.h|utlbuffer\.h|filesystem\.h|'
    r'togles/|togl/|materialsystem/|appframework/'
    r')')

# Objective-C framework imports.
OBJC_IMPORT = re.compile(r'#\s*import\s*[<"]')

# Files that legitimately predate this rule and are known-good because they
# do not include Valve headers at all; listed so the check stays honest if
# someone edits them later.
KNOWN_OK = {
    "appframework/ios_metal_layer.mm",
    "appframework/glmrendererinfo_osx.mm",
    "togl/linuxwin/glmgrcocoa.mm",
    "togles/linuxwin/glmgrcocoa.mm",
}


def main():
    problems = []
    checked = 0

    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in (".git", "thirdparty", "build")]
        for fn in filenames:
            if not fn.endswith(".mm"):
                continue
            path = os.path.join(dirpath, fn)
            rel = os.path.relpath(path, ROOT)
            try:
                text = open(path, errors="ignore").read()
            except OSError:
                continue

            checked += 1

            # Strip comments so a header named in prose does not trip us.
            code = re.sub(r'//.*$', '', text, flags=re.M)
            code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)

            has_objc = bool(OBJC_IMPORT.search(code))
            valve_hits = VALVE_HEADERS.findall(code)

            if has_objc and valve_hits:
                problems.append((rel, sorted(set(valve_hits))))

    print(f"checked {checked} .mm files")

    # --- second check: wscript if/elif chains ---
    # Inserting `if bld.env.MY_FLAG:` immediately above an existing `elif`
    # silently steals that chain. This happened once already: it dropped
    # CFNetwork from the engine link on darwin and failed with undefined
    # _CFNetworkCopyProxiesForURL. Flag any if-block whose condition tests
    # one of our feature flags but is followed by an elif.
    FEATURE_FLAGS = ("METAL", "MODERN_THREADS", "MIMALLOC", "HAPTICS",
                     "PHASE_AUDIO", "PRECOMPILED_SHADERS", "NO_LEGACY_CONSOLES")
    chain_problems = []

    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in (".git", "thirdparty", "build")]
        if "wscript" not in filenames:
            continue
        path = os.path.join(dirpath, "wscript")
        rel = os.path.relpath(path, ROOT)
        lines = open(path, errors="ignore").read().splitlines()

        for i, line in enumerate(lines):
            stripped = line.strip()
            if not stripped.startswith("if "):
                continue
            if not any(f"env.{f}" in stripped for f in FEATURE_FLAGS):
                continue
            # Look ahead past the body for an elif at the same indent.
            indent = len(line) - len(line.lstrip())
            for j in range(i + 1, min(i + 12, len(lines))):
                nxt = lines[j]
                if not nxt.strip():
                    continue
                nxt_indent = len(nxt) - len(nxt.lstrip())
                if nxt_indent < indent:
                    break
                if nxt_indent == indent:
                    if nxt.strip().startswith("elif ") or nxt.strip().startswith("else:"):
                        chain_problems.append((rel, i + 1, stripped))
                    break

    if chain_problems:
        print("\nFAIL: feature-flag `if` hijacks an existing if/elif chain.")
        print("The following branches swallow the platform cases below them:\n")
        for rel, ln, txt in chain_problems:
            print(f"  {rel}:{ln}  {txt}")
        print("\nFix: make the feature check a separate, additive `if` placed")
        print("after the platform chain, not spliced into it.")
        return 1

    if problems:
        print("\nFAIL: Objective-C translation units including Valve headers.")
        print("Valve defines BOOL as int; ObjC defines it as bool. Both in one")
        print("file is a guaranteed compile error on the iOS toolchain.\n")
        for rel, hits in problems:
            print(f"  {rel}")
            for h in hits:
                print(f"      includes {h}...")
        print("\nFix: move the ObjC code into a .mm with no Valve headers and")
        print("expose it through a plain C bridge header (see")
        print("engine/audio/snd_dev_ios_phase_backend.h for the pattern).")
        return 1

    print("ok: no .mm file mixes Objective-C imports with Valve headers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
