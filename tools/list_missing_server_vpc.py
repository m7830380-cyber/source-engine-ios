#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts', 'waifulib'))
import vpc
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
os.chdir(ROOT)
VPC_CONDITIONALS = {'POSIX': 1, 'OSXALL': 1, 'OSX64': 1, 'GL': 1, 'SDL': 1, 'NO_CEG': 1, 'IOS': 1, 'PORTAL2': 1}
VPC_MACROS = {'_DLL_EXT': '.dylib', 'PLATFORM': 'osx64', 'GAMENAME': 'portal2', 'SRVSRCDIR': '.'}
for path in ['game/server/server_portal_base.vpc', 'game/server/server_portal2.vpc']:
    proj = vpc.parse('.', path, VPC_CONDITIONALS, VPC_MACROS)
    missing = [s for s in proj.sources if 'generated_proto' not in s and not os.path.isfile(s)]
    print(path, 'missing', len(missing))
    for s in missing[:15]:
        print(' ', s)
    if len(missing) > 15:
        print('  ...')
