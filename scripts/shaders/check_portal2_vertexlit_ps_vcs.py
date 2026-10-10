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
    ids = set()
    for _ in range(nstatic):
        ids.add(struct.unpack_from("<I", data, off)[0])
        off += 8
    miss = need - ids
    if miss:
        print("FAIL: %s missing static ids %s (fonts need 64, props need 4)" % (path, sorted(miss)))
        return 1
    print("OK: %s has required vertexlit PS static ids %s" % (path, sorted(need)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
