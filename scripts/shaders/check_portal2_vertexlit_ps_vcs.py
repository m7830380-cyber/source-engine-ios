#!/usr/bin/env python3
"""CI gate: bundled vertexlit PS .vcs must contain props (4) and font (64) static ids."""
import struct
import sys


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "shaderout/shaders/fxc/vertexlit_and_unlit_generic_ps20b.vcs"
    need = {4, 64}
    data = open(path, "rb").read()
    if len(data) < 28:
        print("FAIL: %s shorter than VCS header" % path)
        return 1
    nstatic = struct.unpack_from("<i", data, 20)[0]
    off = 28
    record_ids = set()
    for _ in range(nstatic):
        sid = struct.unpack_from("<I", data, off)[0]
        if sid != 0xFFFFFFFF:
            record_ids.add(sid)
        off += 8
    ndup = struct.unpack_from("<I", data, off)[0]
    off += 4
    alias_pairs = []
    for _ in range(ndup):
        aid, src = struct.unpack_from("<II", data, off)
        alias_pairs.append((aid, src))
        off += 8
    if (64, 4) in alias_pairs:
        print(
            "FAIL: %s staticId 64 aliases to 4 (VERTEXCOLOR would run DIFFUSE bytecode)"
            % path
        )
        return 1
    miss = need - record_ids
    if miss:
        print("FAIL: %s missing static id records %s (fonts need 64, props need 4)" % (path, sorted(miss)))
        return 1
    print("OK: %s has required vertexlit PS static id records %s" % (path, sorted(need)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
