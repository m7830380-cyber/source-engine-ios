#!/usr/bin/env python3
import importlib.util
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
os.chdir(ROOT)
sys.path.insert(0, os.path.join(ROOT, 'scripts', 'waifulib'))

import vpc as _vpc  # noqa: E402

VPC_CONDITIONALS = {
    'POSIX': 1, 'OSXALL': 1, 'OSX64': 1, 'GL': 1, 'SDL': 1, 'CSGO': 1, 'NO_CEG': 1, 'IOS': 1,
}
VPC_MACROS = {
    '_DLL_EXT': '.dylib', '_STATICLIB_EXT': '.a', '_IMPLIB_EXT': '.dylib', '_EXE_EXT': '',
    '_SYM_EXT': '.dSYM', 'PLATFORM': 'osx64', 'PLATSUBDIR': '/osx64', 'GAMENAME': 'csgo',
}
PORTAL2_PROJECTS = {
	'client': 'game/client/client_portal2_rubberwar.vpc',
	'server': 'game/server/server_portal2_rubberwar.vpc',
	'matchmaking': 'matchmaking/matchmaking_portal2.vpc',
}
ROOT_PROJECTS = [
    'launcher_main', 'launcher', 'engine', 'filesystem_stdio', 'inputsystem', 'materialsystem',
    'datacache', 'studiorender', 'soundemittersystem', 'vscript', 'vguimatsurface', 'vgui_dll',
    'scaleformui', 'localize', 'togl', 'scenefilecache', 'client', 'server', 'matchmaking',
    'serverbrowser', 'vaudio_speex',
]
PROJECT_OVERRIDES = {'vgui2': 'vgui2/src/vgui_dll.vpc'}
MISSING_LIBS = set([
    'steamdatagramlib', 'blobulator', 'puzzlemaker_lib', 'libcef', 'tcmalloc', 'vtune',
])
EXTERNAL_LIBS = {'sdl2': 'SDL2', 'protobuf': 'PROTOBUF', 'jpeglib': 'JPEG', 'libjpeg': 'JPEG',
                 'png': 'PNG', 'libpng': 'PNG', 'z': 'ZLIB', 'zlib': 'ZLIB',
                 'steam_api': 'steam_api', 'libsteam_api': 'steam_api',
                 'libgfx': 'gfx', 'libgfxplatform': 'gfx', 'libgfx_as2': 'gfx',
                 'libgfxrender_gl': 'gfx', 'libgfxexpat': 'gfx'}
CUSTOM_LIBS = set(['cryptopp', 'gcsdk'])


def _project_map(vpc_mod):
    import re
    ctx = vpc_mod._Ctx(os.path.abspath('.'), os.path.abspath('.'), VPC_CONDITIONALS, VPC_MACROS)
    with open('vpc_scripts/projects.vgc', 'rb') as f:
        text = f.read().decode('latin-1')
    projects = {}
    for m in re.finditer(r'\$Project\s+"([^"]+)"\s*\{(.*?)\}', text, re.S):
        for line in m.group(2).split('\n'):
            line = line.strip()
            if not line.startswith('"'):
                continue
            path = line.split('"')[1].replace('\\', '/')
            cond = re.search(r'\[.*\]', line)
            if cond and not ctx.eval_cond(cond.group(0)):
                continue
            projects.setdefault(m.group(1).lower(), path)
    for k, v in PROJECT_OVERRIDES.items():
        projects[k] = v
    return projects


def _game_conditionals(game, name):
    conds = dict(VPC_CONDITIONALS)
    if game == 'portal2' and name in PORTAL2_PROJECTS:
        del conds['CSGO']
        conds['PORTAL2'] = 1
    return conds


def _load_projects(roots, game='csgo'):
    vpc_mod = _vpc
    pmap = _project_map(vpc_mod)
    if game == 'portal2':
        pmap.update(PORTAL2_PROJECTS)
    parsed = {}
    order = []
    todo = list(roots)
    while todo:
        name = todo.pop(0).lower()
        if name in parsed or name in MISSING_LIBS or name in EXTERNAL_LIBS or name in CUSTOM_LIBS:
            continue
        path = pmap.get(name)
        if not path:
            parsed[name] = None
            continue
        macros = dict(VPC_MACROS)
        macros['PROJECTNAME'] = name
        if game == 'portal2' and name in PORTAL2_PROJECTS:
            macros['GAMENAME'] = 'portal2'
        proj = vpc_mod.parse('.', path, _game_conditionals(game, name), macros)
        parsed[name] = proj
        order.append(name)
        for dep in proj.libs + proj.implibs:
            todo.append(dep)
    return [(n, parsed[n]) for n in order]


if __name__ == '__main__':
    missing = []
    bad_nuts = []
    for name, proj in _load_projects(ROOT_PROJECTS, 'portal2'):
        if proj is None:
            continue
        for s in proj.sources:
            if not os.path.isfile(s):
                missing.append(s)
        for nut in proj.nuts:
            if '$' in nut or not os.path.isfile(nut):
                bad_nuts.append(nut)
    missing = sorted(set(missing))
    print('missing sources:', len(missing))
    for s in missing:
        print(s)
    print('bad nuts:', len(bad_nuts))
    for n in bad_nuts:
        print(n)
