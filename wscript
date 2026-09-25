#! /usr/bin/env python
# encoding: utf-8
#
# CS:GO (cstrike15) iOS build.
#
# Valve's .vpc project scripts are the source of truth for what goes into each
# module: scripts/waifulib/vpc.py reads them and build() turns every project
# into a waf task generator. The iOS toolchain, SDL2/ANGLE and dependency
# handling come from nillerusr's source-engine port.

from __future__ import print_function
from waflib import Logs, Context, Configure, Utils, Errors
import sys
import os
import re
import subprocess

VERSION = '1.0'
APPNAME = 'csgo'
top = '.'

Context.Context.line_just = 55

# VPC conditionals for the iOS target. iOS rides on CS:GO's OSX64
# configuration (Darwin, clang, libc++, togl GL backend); IOS marks the places
# that need UIKit/GLES instead of AppKit/desktop GL. NO_STEAM is deliberately
# not set: CS:GO's client/server never built without Steam, so they link
# stub_steam, an offline Steam client with a single logged-in user.
VPC_CONDITIONALS = {
	'POSIX': 1,
	'OSXALL': 1,
	'OSX64': 1,
	'GL': 1,
	'SDL': 1,
	'CSGO': 1,
	'NO_CEG': 1,
	'IOS': 1,
}

VPC_MACROS = {
	'_DLL_EXT': '.dylib',
	'_STATICLIB_EXT': '.a',
	'_IMPLIB_EXT': '.dylib',
	'_EXE_EXT': '',
	'_SYM_EXT': '.dSYM',
	'PLATFORM': 'osx64',
	'PLATSUBDIR': '/osx64',
	'GAMENAME': 'csgo',
}

# The runtime modules (loaded with dlopen by the launcher/engine) plus the
# executable. Static libraries are pulled in through VPC link dependencies.
ROOT_PROJECTS = [
	'launcher_main',
	'launcher',
	'engine',
	'filesystem_stdio',
	'inputsystem',
	'materialsystem', # on OSX64/iOS it links shaderapidx9, shaderlib and stdshaders in
	'datacache',
	'studiorender',
	'soundemittersystem',
	'vscript',
	'vguimatsurface',
	'vgui_dll',
	'scaleformui', # the real CS:GO Scaleform integration, on thirdparty/scaleform
	'localize',
	'togl',
	'scenefilecache',
	'client',
	'server',
	'matchmaking',
	'serverbrowser',
]

# VPC link names that differ from their projects.vgc project name.
PROJECT_OVERRIDES = {
	'vgui2': 'vgui2/src/vgui_dll.vpc',
}

# VPC link names that are not VPC projects -> waf task/uselib names.
EXTERNAL_LIBS = {
	'sdl2': 'SDL2',
	'protobuf': 'PROTOBUF',
	'jpeglib': 'JPEG',
	'libjpeg': 'JPEG',
	'png': 'PNG',
	'libpng': 'PNG',
	'z': 'ZLIB',
	'zlib': 'ZLIB',
	'steam_api': 'steam_api',
	'libsteam_api': 'steam_api',
	# Scaleform: CS:GO links the prebuilt GFx libraries; we build them as one
	# static library from thirdparty/scaleform (build_scaleform)
	'libgfx': 'gfx',
	'libgfxplatform': 'gfx',
	'libgfx_as2': 'gfx',
	'libgfxrender_gl': 'gfx',
	'libgfxexpat': 'gfx',
}

# Static libraries that CS:GO links as prebuilt binaries but whose sources
# are in the tree; built by build_custom_projects().
CUSTOM_LIBS = set([
	'cryptopp',
	'gcsdk', # gcsdk/gcsdk_ios.cpp: the part of the missing GC SDK the game uses
])

# Extra link dependencies per VPC project on iOS.
PROJECT_EXTRA_USES = {
	'vguimatsurface': ['fontconfig', 'FT2', 'PNG', 'ZLIB'], # linuxfont.cpp font lookup
	'engine': ['CURL'],
}

# Extra compiler flags per VPC project on iOS (appended after the global ones).
PROJECT_EXTRA_CXXFLAGS = {
	# Scaleform's headers in thirdparty/scaleform use C++17 library features
	'scaleformui': ['-std=gnu++17', '-Wno-register', '-Wno-deprecated-register', '-fno-delete-null-pointer-checks'],
}

# Extra sources per VPC project on iOS.
PROJECT_EXTRA_SOURCES = {
	# prebuilt libraries missing from the tree: Steam Datagram Relay, Steam Audio
	'engine': ['ios/engine/steamdatagram_null.cpp', 'ios/engine/phonon_null.cpp'],
	# CSteamID::Render; Valve's engine compiles this in, matchmaking needs it too
	'matchmaking': ['common/steamid.cpp'],
	'client': ['common/steamid.cpp',
		# touch controls, from the source-engine port
		'game/client/touch.cpp', 'game/client/in_touch.cpp',
		# touch buy menu (CS:GO's Scaleform one is not in the source)
		'game/client/cstrike15/VGUI/ios_buymenu.cpp',
		# CS:GO buy wheel glue, reconstructed (cstrike15-restoration)
		'game/client/cstrike15/Scaleform/buymenu_scaleform.cpp',
		# CS:GO pause menu glue, reconstructed (cstrike15-restoration)
		'game/client/cstrike15/Scaleform/pausemenuscreen_scaleform.cpp',
		# offline with bots dialog glue (single-player.swf)
		'game/client/cstrike15/Scaleform/singleplayergamedialog_scaleform.cpp',
		# _global.CScaleformComponent_GameTypes (map group / game mode data for the SWFs)
		'game/client/cstrike15/Scaleform/gametypes_component_scaleform.cpp'],
	# togl/launcher calls without Scaleform's GLES headers in the same file
	'scaleformui': ['scaleformui/scaleformuiimpl/sf_togl_bridge.cpp'],
	# SDL finger events -> IE_Finger* input events
	'inputsystem': ['ios/inputsystem/touch_sdl.cpp'],
	'server': ['common/steamid.cpp'],
}

# VPC link dependencies whose source is not part of the leak. Code that
# needs them is compiled out or stubbed.
MISSING_LIBS = set([
	'steamdatagramlib',
	'libcef',
	'tcmalloc',
	'vtune',
])

# Defines from the VPC scripts that must not reach the iOS build.
# INCLUDE_SCALEFORM stays: the game calls IScaleformUI in many unguarded places,
# so iOS loads scaleformui/null instead (Scaleform GFx itself is a prebuilt
# x86 library).
DROP_DEFINES = set([
])

# Platform defines CS:GO's POSIX/OSX64 VPC base scripts give every project;
# set globally so non-VPC subprojects (vphysics, ivp) see the same platform.
PLATFORM_DEFINES = [
	'POSIX', '_POSIX', 'OSX', '_OSX', 'GNUC', 'COMPILER_GCC',
	'PLATFORM_64BITS', 'USE_SDL', 'DX_TO_GL_ABSTRACTION', 'GL_GLEXT_PROTOTYPES',
	'CSTRIKE15', 'CSTRIKE_REL_BUILD=1', 'RAD_TELEMETRY_DISABLED',
	'_DARWIN_UNLIMITED_SELECT', 'FD_SETSIZE=10240', '_DLL_EXT=.dylib',
]

IOS_DEFINES = [
	'APPLE=1', '_APPLE=1', # expected by togl (the port's GLES togles)
	'IOS=1',
	'_IOS=1',
	'PLATFORM_IOS=1',
	'NO_CEG=1',
	'TOGLES=1', # togl is the source-engine port's GLES backend
	'IOS_DEFAULT_GAME="csgo"', # launch dialog default (launcher_main/ios)
	# A global operator new/delete exported from our dylibs replaces the one
	# every system framework uses (dyld coalesces weak C++ operators), so
	# e.g. MetalPerformanceShadersGraph allocated through tier0 before tier0
	# was initialized and crashed at launch. Same choice as the port.
	'NO_MEMOVERRIDE_NEW_DELETE=1',
]

# VPC include dirs replaced by the build's own copies.
# VPC include dirs that point at paths we lay out differently.
INCLUDE_REMAP = {
	'thirdparty/scaleform/sdk42/Include': 'thirdparty/scaleform/Include',
	'thirdparty/scaleform/sdk42/Src': 'thirdparty/scaleform/Src',
}

DROP_INCLUDES = set([
	'thirdparty/SDL2',  # CS:GO's bundled SDL2 headers; we use the SDL we link
])

IOS_FRAMEWORKS = [
	'Foundation', 'CoreFoundation', 'UIKit', 'QuartzCore', 'CoreGraphics',
	'CoreAudio', 'AudioToolbox', 'AVFoundation', 'OpenAL', 'GameController',
	'CoreMotion', 'CoreHaptics', 'Metal', 'SystemConfiguration', 'CFNetwork',
	'Security',
]

@Configure.conf
def get_taskgen_count(self):
	try: idx = self.tg_idx_count
	except: idx = 0 # don't set tg_idx_count to not increase counter
	return idx

def options(opt):
	grp = opt.add_option_group('Common options')

	grp.add_option('-D', '--debug-engine', action = 'store_true', dest = 'DEBUG_ENGINE', default = False,
		help = 'build with -DDEBUG [default: %default]')

	grp.add_option('--disable-warns', action = 'store_true', dest = 'DISABLE_WARNS', default = False,
		help = 'disable compiler warnings [default: %default]')

	grp.add_option('--ios', action = 'store_true', dest = 'IOS', default = False,
		help = 'build for iOS [default: %default]')

	grp.add_option('--simulator', action = 'store_true', dest = 'IOSSIM', default = False,
		help = 'build for iOS simulator (Use with --ios) [default: %default]')

	grp.add_option('--angle', action = 'store_true', dest = 'ANGLE', default = False,
		help = 'use ANGLE (GLES over Metal) instead of native OpenGLES [default: %default]')

	grp.add_option('--togles', action = 'store_true', dest = 'TOGLES', default = False,
		help = 'accepted for compatibility with the CI script [default: %default]')

	grp.add_option('--build-games', action = 'store', dest = 'GAMES', type = 'string', default = 'csgo',
		help = 'accepted for compatibility with the CI script [default: %default]')

	grp.add_option('--projects', action = 'store', dest = 'PROJECTS', type = 'string', default = '',
		help = 'comma separated root projects to build instead of the full game [default: all]')

	grp.add_option('--use-ccache', action = 'store_true', dest = 'CCACHE', default = False,
		help = 'build using ccache [default: %default]')

	grp.add_option('--protoc', action = 'store', dest = 'PROTOC', type = 'string', default = '',
		help = 'host protoc 2.5.0 used to generate protobuf sources [default: search PATH]')

	opt.load('compiler_optimizations subproject')
	opt.load('xcompile compiler_cxx compiler_c sdl2 clang_compilation_database strip_on_install_v2 subproject')
	opt.load('reconfigure')

def configure(conf):
	conf.load('fwgslib reconfigure compiler_optimizations')

	if not conf.options.IOS:
		conf.fatal('This tree only supports the iOS build (--ios)')

	conf.load('subproject xcompile compiler_c compiler_cxx gccdeps gitversion clang_compilation_database strip_on_install_v2 enforce_pic mm_hook')

	conf.env.IOS = 1
	conf.env.ANGLE = conf.options.ANGLE
	conf.env.PROJECTS = conf.options.PROJECTS

	protoc = conf.options.PROTOC or os.environ.get('PROTOC', '')
	if protoc:
		conf.env.PROTOC = [os.path.abspath(protoc)]
	else:
		conf.find_program('protoc', var = 'PROTOC')

	defines = PLATFORM_DEFINES + IOS_DEFINES
	if conf.options.ANGLE:
		defines += ['ANGLE=1']
	defines += ['DEBUG', '_DEBUG'] if conf.options.DEBUG_ENGINE else ['NDEBUG']
	conf.env.append_unique('DEFINES', defines)

	cflags, linkflags = conf.get_optimization_flags()

	flags = [
		'-pipe', '-fPIC', '-pthread',
		'-fsigned-char',
		'-fvisibility=hidden',
		'-fno-strict-aliasing',
		'-L' + os.path.abspath('lib/darwin/aarch64'),
	]
	if conf.options.DISABLE_WARNS:
		flags += ['-w']

	# Valve's code predates C++11 narrowing rules and newer clang defaults
	# that turn old-style C into hard errors.
	compat = [
		'-Wno-c++11-narrowing',
		'-Wno-reserved-user-defined-literal',
		'-Wno-register',
		'-Wno-error=implicit-function-declaration',
		'-Wno-error=int-conversion',
		'-Wno-error=incompatible-pointer-types',
		'-Wno-error=incompatible-function-pointer-types',
		'-Wno-error=enum-constexpr-conversion',
		'-Wno-error=non-pod-varargs',
		'-Wno-error=address-of-temporary',
		'-Wno-error=invalid-offsetof',
		'-Wno-error=return-type',
		'-Wno-error=format-security',
	]

	cflags += flags
	linkflags += flags

	cxxflags = list(cflags) + ['-std=gnu++11']

	cflags += conf.filter_cflags(compat, cflags)
	cxxflags += conf.filter_cxxflags(compat, cxxflags)

	conf.env.append_unique('CFLAGS', cflags)
	conf.env.append_unique('CXXFLAGS', cxxflags)
	conf.env.append_unique('LINKFLAGS', linkflags)

	# compatibility headers for glibc/Windows-isms (e.g. <malloc.h>)
	conf.env.append_unique('INCLUDES', [os.path.abspath('ios/include')])
	# FreeType/fontconfig headers for vgui_surfacelib's linuxfont.cpp
	conf.env.append_unique('INCLUDES', [
		os.path.abspath('lib/darwin/aarch64/include'),
		os.path.abspath('ios/thirdparty/freetype/include'),
		os.path.abspath('ios/thirdparty/fontconfig'),
	])
	# EGL/KHR headers for the ANGLE context in appframework/sdlmgr.cpp
	conf.env.append_unique('INCLUDES', [os.path.abspath('ios/thirdparty/SDL-src/src/video/khronos')])

	check_deps(conf)

	conf.load('sdl2')
	if not conf.env.HAVE_SDL2:
		conf.fatal("SDL2 isn't available")
	for inc in conf.env.INCLUDES_SDL2:
		conf.env.append_unique('INCLUDES', inc)
	if conf.options.SDL2_PATH:
		sdl_headers = os.path.abspath(os.path.join(conf.options.SDL2_PATH, 'Headers'))
		sdl_source_headers = os.path.abspath('ios/thirdparty/SDL-src/include')
		conf.env.append_unique('INCLUDES', sdl_source_headers)
		conf.env.append_unique('INCLUDES', sdl_headers)

	conf.env.LIBDIR = conf.env.BINDIR = conf.env.PREFIX

	if conf.options.CCACHE:
		conf.env.CC.insert(0, 'ccache')
		conf.env.CXX.insert(0, 'ccache')

	conf.add_subproject(['ivp/havana', 'ivp/havana/havok/hk_base', 'ivp/havana/havok/hk_math',
		'ivp/ivp_compact_builder', 'ivp/ivp_physics', 'vphysics'])

def check_deps(conf):
	conf.env.FRAMEWORK_IOS = list(IOS_FRAMEWORKS)
	conf.env.FRAMEWORK_SDL2 = ['SDL2']
	if conf.env.ANGLE:
		angle_fw_path = os.environ.get('ANGLE_FRAMEWORK_PATH', os.path.abspath('build/ios'))
		conf.env.FRAMEWORK_GLES = ['libEGL', 'libGLESv2']
		conf.env.FRAMEWORKPATH_GLES = [angle_fw_path]
		conf.env.LINKFLAGS += ['-F' + angle_fw_path]
	else:
		conf.env.FRAMEWORK_GLES = ['OpenGLES']

	conf.check(lib='z', uselib_store='ZLIB')
	conf.check(lib='bz2', uselib_store='BZ2')
	conf.check(lib='iconv', uselib_store='ICONV')
	conf.check(lib='jpeg', uselib_store='JPEG')
	conf.check(lib='png', uselib_store='PNG')
	conf.check(lib='freetype2', uselib_store='FT2')
	conf.check(lib='protobuf', uselib_store='PROTOBUF')
	conf.check(lib='curl', uselib_store='CURL')

# ---------------------------------------------------------------------------
# VPC driven build

def _vpc():
	sys.path.insert(0, os.path.abspath('scripts/waifulib'))
	import vpc
	return vpc

def _project_map(vpc):
	'''name -> vpc path, from vpc_scripts/projects.vgc evaluated for iOS'''
	ctx = vpc._Ctx(os.path.abspath('.'), os.path.abspath('.'), VPC_CONDITIONALS, VPC_MACROS)
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

def _load_projects(roots):
	vpc = _vpc()
	pmap = _project_map(vpc)
	parsed = {}
	order = []
	todo = list(roots)
	while todo:
		name = todo.pop(0).lower()
		if name in parsed or name in MISSING_LIBS or name in EXTERNAL_LIBS or name in CUSTOM_LIBS:
			continue
		path = pmap.get(name)
		if not path:
			Logs.warn('VPC: no project for %s, skipping' % name)
			parsed[name] = None
			continue
		macros = dict(VPC_MACROS)
		macros['PROJECTNAME'] = name
		proj = vpc.parse('.', path, VPC_CONDITIONALS, macros)
		parsed[name] = proj
		order.append(name)
		for dep in proj.libs + proj.implibs:
			todo.append(dep)
	return [(n, parsed[n]) for n in order]

def _gen_protos(bld, proj):
	if not proj.protos:
		return
	gen = proj.macros.get('GENERATED_PROTO_DIR', 'generated_proto').replace('\\', '/')
	outdir = os.path.normpath(os.path.join(proj.projdir, gen))
	if not os.path.isdir(outdir):
		os.makedirs(outdir)
	protoc = bld.env.PROTOC
	if not protoc:
		bld.fatal('protoc not configured, pass --protoc')
	# always regenerate: waf only rebuilds when the output content changes
	for proto in proj.protos:
		cmd = protoc + [
			'--proto_path=thirdparty/protobuf-2.5.0/src',
			'--proto_path=' + os.path.dirname(proto),
			'--proto_path=gcsdk',
			'--proto_path=game/shared',
			'--proto_path=game/shared/cstrike15',
			'--proto_path=common',
			'--cpp_out=' + outdir,
			proto,
		]
		Logs.info('protoc %s -> %s' % (proto, outdir))
		subprocess.check_call(cmd)

def _gen_nuts(proj):
	# Valve's devtools/bin/texttoarray.pl: embed a squirrel script as a
	# NUL-terminated byte array named g_Script_<name>, next to the script.
	for nut in proj.nuts:
		base = os.path.splitext(os.path.basename(nut))[0]
		out = os.path.join(os.path.dirname(nut), base + '_nut.h')
		with open(nut, 'rb') as f:
			data = bytearray(f.read())
		lines = ['static unsigned char g_Script_%s[] = {' % base]
		for i in range(0, len(data), 20):
			lines.append('    ' + ''.join('0x%02x,' % b for b in data[i:i + 20]))
		lines.append('    0x00')
		lines.append('};')
		text = '\n'.join(lines) + '\n'
		if not os.path.exists(out) or open(out).read() != text:
			with open(out, 'w') as f:
				f.write(text)

def _uses(names):
	out = []
	for n in names:
		low = n.lower()
		if low in MISSING_LIBS:
			continue
		out.append(EXTERNAL_LIBS.get(low, low))
	return out

def _defines(proj):
	out = []
	for d in proj.defines:
		key = d.split('=')[0]
		if key in DROP_DEFINES or '$' in d:
			continue
		out.append(d)
	out.append('MEMOVERRIDE_MODULE=%s' % proj.macros.get('PROJECTNAME', proj.name))
	return out

CRYPTOPP_DIR = 'external/crypto++-5.61'
# test/benchmark programs from the GNUmakefile's TESTOBJS
CRYPTOPP_EXCLUDE = set(['bench.cpp', 'bench2.cpp', 'test.cpp', 'validat1.cpp',
	'validat2.cpp', 'validat3.cpp', 'adhoc.cpp', 'datatest.cpp', 'regtest.cpp',
	'fipsalgt.cpp', 'dlltest.cpp'])

SCALEFORM_DIR = 'thirdparty/scaleform'

def _scaleform_sources(listname):
	# Scaleform's make lists: files below a "[plat,...]" header only build on
	# those platforms, "[-plat,...]" means all but those. iOS is in none of the
	# positive lists, so take the unconditional part plus the negated sections.
	path = os.path.join(SCALEFORM_DIR, 'Projects', listname + '.txt')
	sources, include = [], True
	for line in open(path).read().splitlines():
		line = line.strip()
		if not line:
			continue
		if line.startswith('['):
			include = line.startswith('[-')
			continue
		if include and line.endswith(('.cpp', '.c')) and '/AMP/' not in line:
			src = os.path.join(SCALEFORM_DIR, line)
			if os.path.exists(src):
				sources.append(src)
	return sources

SCALEFORM_DEFINES = [
	'SF_USE_GLES2',  # GL renderer in GLES2 mode, on ANGLE (GLES 3.0)
	'SF_USE_ANGLE',  # ANGLE's GLES2 headers, not the OpenGLES framework
	'SF_USE_EGL',    # load GL extension entry points via eglGetProcAddress
	'SF_BUILD_SHIPPING', 'NDEBUG',
]

def build_scaleform(bld):
	# Scaleform GFx 4.x (CS:GO shipped 4.2.23): Kernel, Render, GFx core with
	# the AS2 VM (CS:GO's menus/HUD are ActionScript 2) and the GL renderer.
	sources = _scaleform_sources('libgfx') + _scaleform_sources('libgfx_as2') + \
		_scaleform_sources('libgfxrender_gl') + [SCALEFORM_DIR + '/Src/Kernel/SF_ThreadsPthread.cpp',
		# desktop GLSL 1.10 shader tables, run as GLSL ES 1.00 on ANGLE (see GL_Shader.cpp)
		SCALEFORM_DIR + '/Src/Render/GL/GL_ShaderDescs.cpp', SCALEFORM_DIR + '/Src/Render/GL/GL_ShaderSource.cpp',
		# GPU fences for the renderer (not in the SDK's GL list)
		SCALEFORM_DIR + '/Src/Render/GL/GL_Sync.cpp']
	sources = sorted(set(sources))
	env = bld.env.derive()
	# this Scaleform tree targets C++20 (later -std wins over the global gnu++11)
	# Scaleform relies on null-pointer idioms (e.g. *(p ? q : 0)) that clang would
	# otherwise treat as proof of non-null and drop the checks
	env.append_value('CXXFLAGS', ['-std=gnu++20', '-w', '-fno-delete-null-pointer-checks'])
	env.append_value('CFLAGS', ['-w'])
	bld(
		features = 'c cxx cstlib cxxstlib',
		source   = sources,
		env      = env,
		target   = 'gfx',
		name     = 'gfx',
		install_path = None, # static; linked into scaleformui
		includes = [SCALEFORM_DIR + '/Include', SCALEFORM_DIR + '/Src'],
		export_includes = [SCALEFORM_DIR + '/Include', SCALEFORM_DIR + '/Src'],
		defines  = SCALEFORM_DEFINES,
		export_defines = SCALEFORM_DEFINES,
		use      = ['GLES', 'ZLIB', 'PNG', 'JPEG'],
	)

def build_custom_projects(bld):
	build_scaleform(bld)

	# scaleformui is now the real integration, built from scaleformui.vpc
	# (ROOT_PROJECTS); scaleformui/null/ stays as a fallback.

	env = bld.env.derive()
	env.cxxshlib_PATTERN = 'lib%s.dylib'
	bld(
		features = 'cxx cxxshlib',
		# offline Steam client: CS:GO does not start without a logged-in user
		source   = ['stub_steam/steam_api.cpp', 'stub_steam/steam_offline.cpp'],
		target   = 'steam_api',
		name     = 'steam_api',
		includes = ['stub_steam', 'public', 'public/steam', 'common'],
		env      = env,
		install_path = bld.env.LIBDIR,
	)

	sources = sorted(f for f in os.listdir(CRYPTOPP_DIR)
		if f.endswith('.cpp') and f not in CRYPTOPP_EXCLUDE)
	# fontconfig subset for CS:GO's Linux font code (no fontconfig on iOS)
	bld(
		features = 'c cstlib',
		source   = ['ios/fontconfig/fontconfig_ios.c'],
		target   = 'fontconfig',
		name     = 'fontconfig',
		install_path = None, # static; linked into the modules
		use      = ['FT2'],
	)

	# GC SDK subset plus the steammessages protobuf it builds on
	class _GcsdkProto:
		protos = ['gcsdk/steammessages.proto']
		projdir = 'gcsdk'
		macros = {'GENERATED_PROTO_DIR': 'generated_proto'}
	_gen_protos(bld, _GcsdkProto)
	bld(
		features = 'cxx cxxstlib',
		source   = ['gcsdk/gcsdk_ios.cpp', 'gcsdk/generated_proto/steammessages.pb.cc'],
		target   = 'gcsdk',
		name     = 'gcsdk',
		install_path = None, # static; linked into the modules
		includes = ['gcsdk/generated_proto', 'thirdparty/protobuf-2.5.0/src', 'gcsdk',
			'gcsdk/steamextra', 'common', 'public', 'public/tier0', 'public/tier1', 'public/gcsdk'],
		defines  = ['PROTOBUF'],
		use      = ['PROTOBUF'],
	)

	env = bld.env.derive()
	# Crypto++ 5.6.1 relies on MSVC-style template lookup
	env.append_value('CXXFLAGS', ['-fdelayed-template-parsing'])
	bld(
		features = 'cxx cxxstlib',
		source   = [CRYPTOPP_DIR + '/' + f for f in sources],
		target   = 'cryptopp',
		name     = 'cryptopp',
		install_path = None, # static; linked into the modules
		includes = [CRYPTOPP_DIR],
		export_includes = [CRYPTOPP_DIR],
		defines  = ['CRYPTOPP_DISABLE_ASM', 'CRYPTOPP_DISABLE_SSE2'],
		env      = env,
	)

def build(bld):
	# VPC paths are relative to the source root
	os.chdir(bld.path.abspath())

	bld.add_subproject(['ivp/havana', 'ivp/havana/havok/hk_base', 'ivp/havana/havok/hk_math',
		'ivp/ivp_compact_builder', 'ivp/ivp_physics', 'vphysics'])

	build_custom_projects(bld)

	roots = [p for p in bld.env.PROJECTS.split(',') if p] if bld.env.PROJECTS else ROOT_PROJECTS
	projects = _load_projects(roots)

	# Shared libraries other modules link against (VPC $ImpLib: tier0,
	# vstdlib, togl, ...) get Valve's POSIX "lib" prefix so -l<name> finds
	# them; modules the engine dlopens by name (engine.dylib, client.dylib)
	# do not.
	linked = set()
	for _, proj in projects:
		if proj:
			linked.update(n.lower() for n in proj.implibs)

	for name, proj in projects:
		if proj is None:
			continue
		_gen_protos(bld, proj)
		_gen_nuts(proj)

		sources = [s for s in proj.sources if os.path.exists(s)] + PROJECT_EXTRA_SOURCES.get(name, [])
		missing = [s for s in proj.sources if not os.path.exists(s)]
		for s in missing:
			Logs.warn('%s: missing source %s' % (name, s))

		includes = [INCLUDE_REMAP.get(i, i) for i in proj.includes if i not in DROP_INCLUDES]
		use = _uses(proj.libs + proj.implibs) + PROJECT_EXTRA_USES.get(name, [])

		env = bld.env.derive()
		env.append_value('CXXFLAGS', PROJECT_EXTRA_CXXFLAGS.get(name, []))
		install_path = None
		if proj.kind == 'lib':
			features = 'c cxx cstlib cxxstlib'
			target = name
		elif proj.kind == 'exe':
			features = 'c cxx cprogram cxxprogram'
			target = proj.macros.get('OUTBINNAME', name)
			install_path = bld.env.BINDIR
			use += ['IOS', 'SDL2', 'GLES', 'ZLIB', 'BZ2', 'ICONV']
		else:
			features = 'c cxx cshlib cxxshlib'
			target = proj.macros.get('OUTBINNAME', name)
			env.cshlib_PATTERN = env.cxxshlib_PATTERN = 'lib%s.dylib' if name in linked else '%s.dylib'
			install_path = bld.env.LIBDIR
			use += ['IOS', 'SDL2', 'GLES', 'ZLIB', 'BZ2', 'ICONV']

		bld(
			features = features,
			source   = sources,
			target   = target,
			name     = name,
			includes = includes,
			defines  = _defines(proj),
			use      = use,
			env      = env,
			# no idx: waf numbers task generators itself, which keeps object
			# names unique for sources shared by several modules
			install_path = install_path,
		)
