#!/usr/bin/env python
"""Split shader targets into compile shards of roughly equal fxc work.

usage: plan_shards.py <invocations per shard> <src.fxc:target> ...
Prints a GitHub Actions matrix: {"include": [{"src", "target", "shard", "shards"}, ...]}
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vcscompile as V  # noqa: E402


def main():
    per_shard = int(sys.argv[1])
    include = []
    for spec in sys.argv[2:]:
        src, target = spec.split(':')
        c = V.parse_fxc(os.path.join(V.STDSHADERS, src), target)
        live, num_dyn, n_inv = V.enumerate_live_fast(c)
        shards = max(1, min(128, (n_inv + per_shard - 1) // per_shard))
        sys.stderr.write('%s: %d live static combos, %d fxc invocations -> %d shards\n'
                         % (target, len(live), n_inv, shards))
        for k in range(shards):
            include.append({'src': src, 'target': target, 'shard': k, 'shards': shards})
    print(json.dumps({'include': include}))


if __name__ == '__main__':
    main()
