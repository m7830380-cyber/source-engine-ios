#!/usr/bin/env python3
"""Copy missing Portal 2 VPC sources from RubberWar/Portal-2 when available."""
import os
import shutil
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
PORTAL2_SRC = os.environ.get(
    'PORTAL2_SRC',
    os.path.abspath(os.path.join(ROOT, '..', 'Portal-2', 'src')),
)
sys.path.insert(0, os.path.dirname(__file__))
from list_missing_portal2 import _load_projects, ROOT_PROJECTS  # noqa: E402

DRY = '--dry-run' in sys.argv


def portal2_path(rel):
    return os.path.join(PORTAL2_SRC, rel.replace('/', os.sep))


def main():
    if not os.path.isdir(PORTAL2_SRC):
        print('Portal-2 src not found:', PORTAL2_SRC)
        sys.exit(1)
    missing = []
    for name, proj in _load_projects(ROOT_PROJECTS, 'portal2'):
        if proj is None:
            continue
        for s in proj.sources:
            if 'generated_proto' in s:
                continue
            if not os.path.isfile(os.path.join(ROOT, s)):
                missing.append(s)
    missing = sorted(set(missing))
    copied = 0
    still_missing = []
    for rel in missing:
        src = portal2_path(rel)
        dst = os.path.join(ROOT, rel)
        if os.path.isfile(src):
            if not DRY:
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(src, dst)
            copied += 1
            print('copy', rel)
        else:
            still_missing.append(rel)
    print('---')
    print('copied', copied, 'still missing', len(still_missing))
    for s in still_missing:
        print('MISSING', s)


if __name__ == '__main__':
    main()
