#!/usr/bin/env python3
"""List VPC sources from client_portal_base missing on disk."""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts', 'waifulib'))
import vpc

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
os.chdir(ROOT)

VPC_CONDITIONALS = {
    'POSIX': 1, 'OSXALL': 1, 'OSX64': 1, 'GL': 1, 'SDL': 1, 'CSGO': 1, 'NO_CEG': 1, 'IOS': 1,
    'PORTAL2': 1,
}
VPC_MACROS = {
    '_DLL_EXT': '.dylib', 'PLATFORM': 'osx64', 'GAMENAME': 'portal2',
}
conds = dict(VPC_CONDITIONALS)
del conds['CSGO']
conds['PORTAL2'] = 1
proj = vpc.parse('.', 'game/client/client_portal_base.vpc', conds, VPC_MACROS)
missing = [s for s in proj.sources if not os.path.isfile(s)]
print('client_portal_base missing', len(missing))
for s in missing:
    print(s)
