#!/usr/bin/env python3
"""Pack a directory tree into a single-file VPK v2.

    python3 scripts/ios/pack_dir_vpk.py <dir> <out_dir.vpk>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from touch_icons import write_vpk  # noqa: E402


def main():
    src, out = sys.argv[1], sys.argv[2]
    entries = {}
    for root, _, files in os.walk(src):
        for fn in files:
            if fn.lower().endswith('.md') or '.' not in fn:
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, src).replace(os.sep, '/').lower()
            entries[rel] = open(path, 'rb').read()
    write_vpk(out, entries)
    print('%s: %d files' % (out, len(entries)))


if __name__ == '__main__':
    main()
