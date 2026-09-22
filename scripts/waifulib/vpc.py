# encoding: utf-8
# Minimal Valve VPC project reader used by the waf build.
#
# Understands enough of the VPC language to extract what a waf task generator
# needs from CS:GO's .vpc scripts: source files, preprocessor definitions,
# include dirs, static/import library dependencies, system frameworks and the
# project kind (dll/lib/exe). $Include, $Macro, $Conditional, [conditions],
# -$File and per-file $ExcludedFromBuild are honoured. Build steps, custom
# tools and IDE-only settings are ignored.

import os
import re

_TOKEN_RE = re.compile(r'''
	(?P<ws>[ \t\r]+) |
	(?P<comment>//[^\n]*) |
	(?P<nl>\n) |
	(?P<lbrace>\{) |
	(?P<rbrace>\}) |
	(?P<cond>\[[^\]\n]*\]) |
	(?P<str>"[^"\n]*"?) |   # no escapes in VPC: "..\" is a path ending in a backslash
	(?P<word>[^\s{}\[\]"]+)
''', re.X)

_COND_TOKEN_RE = re.compile(r'\s*(&&|\|\||!|\(|\)|\$?[A-Za-z0-9_.]+)')

class VPCError(Exception):
	pass

def _tokenize(text):
	# join line continuations
	text = re.sub(r'\\[ \t]*\r?\n', ' ', text)
	toks = []
	for m in _TOKEN_RE.finditer(text):
		kind = m.lastgroup
		if kind in ('ws', 'comment'):
			continue
		val = m.group(kind)
		if kind == 'str':
			val = val[1:-1] if val.endswith('"') and len(val) > 1 else val[1:]
		toks.append((kind, val))
	return toks

class _Stmt(object):
	__slots__ = ('key', 'args', 'arg_conds', 'cond', 'block')
	def __init__(self, key, args, arg_conds, cond, block):
		self.key = key
		self.args = args
		# condition written right after each argument; a $File statement
		# listing several files (joined with \) has one per file
		self.arg_conds = arg_conds
		self.cond = cond
		self.block = block

def _parse_block(toks, pos):
	stmts = []
	n = len(toks)
	while pos < n:
		kind, val = toks[pos]
		if kind == 'nl':
			pos += 1
			continue
		if kind == 'rbrace':
			return stmts, pos + 1
		if kind == 'lbrace':
			# anonymous block, parse and discard
			_, pos = _parse_block(toks, pos + 1)
			continue
		key = val
		pos += 1
		args = []
		arg_conds = []
		cond = None
		while pos < n and toks[pos][0] in ('str', 'word', 'cond'):
			if toks[pos][0] == 'cond':
				cond = toks[pos][1]
				if arg_conds:
					arg_conds[-1] = cond
			else:
				args.append(toks[pos][1])
				arg_conds.append(None)
			pos += 1
		# a block may start on the following line(s)
		look = pos
		while look < n and toks[look][0] == 'nl':
			look += 1
		# only treat "{" as ours if no other statement intervened
		block = None
		if look < n and toks[look][0] == 'lbrace':
			if look == pos or key.startswith('$') or key.startswith('-$'):
				block, pos = _parse_block(toks, look + 1)
				# condition can also come after the block name on next line: rare
		stmts.append(_Stmt(key, args, arg_conds, cond, block))
	return stmts, pos

class VPCProject(object):
	def __init__(self):
		self.name = None
		self.kind = None # 'dll', 'lib', 'exe'
		self.sources = [] # paths relative to root (posix separators)
		self.defines = []
		self.includes = [] # paths relative to root
		self.libs = [] # static lib base names
		self.implibs = [] # shared lib base names
		self.frameworks = []
		self.syslibs = []
		self.macros = {}
		self.vpc_files = []
		self.protos = [] # .proto files relative to root
		self.nuts = [] # squirrel scripts embedded as <name>_nut.h
		self.projdir = '' # project directory relative to root

	def __repr__(self):
		return '<VPCProject %s kind=%s sources=%d>' % (self.name, self.kind, len(self.sources))

class _Ctx(object):
	def __init__(self, root, projdir, conditionals, macros):
		self.root = root
		self.projdir = projdir
		self.cond = dict((k.upper(), v) for k, v in conditionals.items())
		self.macros = dict((k.upper(), v) for k, v in macros.items())
		self.proj = VPCProject()
		self.removed = set()
		self.config = 'release'

	# ---- conditions ----
	def cond_value(self, name):
		name = name.lstrip('$').upper()
		if name in ('1', 'TRUE'): return True
		if name in ('0', 'FALSE'): return False
		v = self.cond.get(name)
		if v is None:
			return False
		if isinstance(v, bool):
			return v
		return str(v) not in ('', '0')

	def eval_cond(self, text):
		if text is None:
			return True
		expr = text.strip()[1:-1]
		toks = []
		pos = 0
		while pos < len(expr):
			m = _COND_TOKEN_RE.match(expr, pos)
			if not m:
				if expr[pos:].strip() == '':
					break
				pos += 1
				continue
			toks.append(m.group(1))
			pos = m.end()
		if not toks:
			return True
		i = [0]
		def peek():
			return toks[i[0]] if i[0] < len(toks) else None
		def take():
			t = peek(); i[0] += 1; return t
		def atom():
			t = take()
			if t is None: return False
			if t == '!': return not atom()
			if t == '(':
				v = orx()
				if peek() == ')': take()
				return v
			return self.cond_value(t)
		def andx():
			v = atom()
			while peek() == '&&':
				take(); r = atom(); v = v and r
			return v
		def orx():
			v = andx()
			while peek() == '||':
				take(); r = andx(); v = v or r
			return v
		return orx()

	# ---- macros ----
	def expand(self, s, base=None):
		if '$' not in s:
			return s
		def rep(m):
			name = m.group(1).upper()
			if name == 'BASE':
				return base if base is not None else ''
			if name in self.macros:
				return self.macros[name]
			if name in self.cond:
				return str(self.cond[name])
			return m.group(0)
		# expand repeatedly for nested macros
		for _ in range(8):
			ns = re.sub(r'\$([A-Za-z_][A-Za-z0-9_]*)', rep, s)
			if ns == s:
				break
			s = ns
		return s

	def path(self, p):
		p = self.expand(p).replace('\\', '/')
		if not os.path.isabs(p):
			p = os.path.join(self.projdir, p)
		p = os.path.normpath(p)
		return _fix_case(p)

	def rel(self, p):
		return os.path.relpath(p, self.root).replace('\\', '/')

def _fix_case(p):
	# Valve's scripts assume a case-insensitive filesystem.
	if os.path.exists(p):
		return p
	parts = os.path.normpath(p).split(os.sep)
	cur = parts[0] + os.sep if parts[0] else os.sep
	if parts[0].endswith(':'):
		cur = parts[0] + os.sep
	for part in parts[1:]:
		if part in ('', '.'):
			continue
		cand = os.path.join(cur, part)
		if os.path.exists(cand):
			cur = cand
			continue
		try:
			entries = os.listdir(cur)
		except OSError:
			return p
		low = part.lower()
		for e in entries:
			if e.lower() == low:
				cur = os.path.join(cur, e)
				break
		else:
			return p
	return cur

_CODE_EXTS = ('.c', '.cc', '.cpp', '.cxx', '.mm', '.m')

def _k(key):
	return key.lower()

# statements that list several items, each with its own [condition]
_LIST_KEYS = ('$file', '$dynamicfile', '$schemafile', '$dynamicfile_nopch', '-$file',
	'$lib', '$implib', '$libexternal', '$implibexternal',
	'-$lib', '-$implib', '-$libexternal', '-$implibexternal')

def _run(ctx, stmts, scope):
	for st in stmts:
		key = _k(st.key)
		if key in _LIST_KEYS:
			args = [a for a, c in zip(st.args, st.arg_conds) if c is None or ctx.eval_cond(c)]
			if not args:
				continue
			st = _Stmt(st.key, args, [None] * len(args), None, st.block)
		elif st.cond is not None and not ctx.eval_cond(st.cond):
			continue

		if key in ('$macro', '$macrorequired', '$macrorequiredallowempty', '$macroemptystring'):
			if not st.args:
				continue
			name = st.args[0].upper()
			val = ctx.expand(st.args[1]) if len(st.args) > 1 else ''
			if key.startswith('$macrorequired'):
				if name not in ctx.macros or ctx.macros[name] == '':
					if len(st.args) > 1:
						ctx.macros[name] = val
					else:
						ctx.macros.setdefault(name, '')
			else:
				ctx.macros[name] = val
		elif key == '$conditional':
			if st.args:
				name = st.args[0].lstrip('$').upper()
				val = st.args[1] if len(st.args) > 1 else '1'
				ctx.cond[name] = val
		elif key == '$include':
			if st.args:
				p = ctx.path(st.args[0])
				_parse_file(ctx, p)
		elif key == '$configuration':
			name = st.args[0].lower() if st.args else None
			if name and name != ctx.config:
				continue
			if st.block:
				_run(ctx, st.block, 'config')
		elif key in ('$compiler', '$linker', '$librarian', '$general', '$gcc', '$posix'):
			if st.block:
				_run(ctx, st.block, key)
		elif key == '$preprocessordefinitions':
			if st.args:
				cur = ';'.join(ctx.proj.defines)
				val = ctx.expand(st.args[0], base=cur)
				ctx.proj.defines = _uniq([d.strip() for d in val.split(';') if d.strip()])
		elif key == '$additionalincludedirectories':
			if st.args:
				val = ctx.expand(st.args[0], base='')
				for d in re.split(r'[;,]', val):
					d = d.strip()
					if not d or '$' in d:
						continue
					ctx.proj.includes.append(ctx.rel(ctx.path(d)))
				ctx.proj.includes = _uniq(ctx.proj.includes)
		elif key in ('$systemframeworks',):
			if st.args:
				for f in ctx.expand(st.args[0], base='').split(';'):
					f = f.strip()
					if f and f not in ctx.proj.frameworks:
						ctx.proj.frameworks.append(f)
		elif key in ('$systemlibraries',):
			if st.args:
				for f in ctx.expand(st.args[0], base='').split(';'):
					f = f.strip()
					if f and f not in ctx.proj.syslibs:
						ctx.proj.syslibs.append(f)
		elif key == '$project':
			if st.args and not ctx.proj.name:
				ctx.proj.name = ctx.expand(st.args[0])
			if st.block:
				_run(ctx, st.block, 'project')
		elif key == '$folder':
			if st.block:
				_run(ctx, st.block, 'project')
		elif key in ('$file', '$dynamicfile', '$schemafile', '$dynamicfile_nopch'):
			excluded = False
			if st.block and _file_excluded(ctx, st.block):
				excluded = True
			for a in st.args:
				p = ctx.rel(ctx.path(a))
				if excluded:
					ctx.removed.add(p.lower())
					continue
				if p.lower().endswith(_CODE_EXTS):
					ctx.proj.sources.append(p)
				elif p.lower().endswith('.proto'):
					ctx.proj.protos.append(p)
				elif p.lower().endswith('.nut'):
					ctx.proj.nuts.append(p)
		elif key == '-$file':
			for a in st.args:
				ctx.removed.add(ctx.rel(ctx.path(a)).lower())
		elif key in ('$lib', '$implib', '$libexternal', '$implibexternal'):
			for a in st.args:
				name = os.path.basename(ctx.expand(a).replace('\\', '/'))
				name = re.sub(r'\.(lib|a|so|dylib)$', '', name)
				if name.startswith('lib') and key.endswith('external'):
					name = name[3:]
				name = re.sub(r'_(i486|486|x64)$', '', name)
				if key.startswith('$implib'):
					ctx.proj.implibs.append(name)
				else:
					ctx.proj.libs.append(name)
		elif key in ('-$lib', '-$implib', '-$libexternal', '-$implibexternal'):
			for a in st.args:
				name = os.path.basename(ctx.expand(a).replace('\\', '/'))
				for lst in (ctx.proj.libs, ctx.proj.implibs):
					while name in lst:
						lst.remove(name)
		else:
			# unknown / uninteresting statement; descend into blocks that
			# can carry files (e.g. $Folder variants) but not tool blocks
			pass

def _file_excluded(ctx, block):
	for st in block:
		k = _k(st.key)
		if st.cond is not None and not ctx.eval_cond(st.cond):
			continue
		if k == '$excludedfrombuild' and st.args and st.args[0].lower().startswith('yes'):
			return True
		if st.block and k in ('$configuration', '$compiler'):
			name = st.args[0].lower() if (k == '$configuration' and st.args) else None
			if name and name != ctx.config:
				continue
			if _file_excluded(ctx, st.block):
				return True
	return False

def _uniq(lst):
	out = []
	seen = set()
	for x in lst:
		if x not in seen:
			seen.add(x)
			out.append(x)
	return out

def _parse_file(ctx, path):
	if not os.path.exists(path):
		raise VPCError('VPC file not found: %s' % path)
	ctx.proj.vpc_files.append(path)
	base = os.path.basename(path).lower()
	if base.startswith('source_dll_base'):
		ctx.proj.kind = 'dll'
	elif base.startswith('source_lib_base'):
		ctx.proj.kind = 'lib'
	elif base.startswith('source_exe') and ctx.proj.kind is None:
		ctx.proj.kind = 'exe'
	with open(path, 'rb') as f:
		text = f.read().decode('latin-1')
	stmts, _ = _parse_block(_tokenize(text), 0)
	_run(ctx, stmts, 'top')

def parse(root, vpc, conditionals, macros=None, config='release'):
	'''Parse <root>/<vpc>. Returns a VPCProject whose paths are relative to root.'''
	root = os.path.abspath(root)
	path = _fix_case(os.path.join(root, vpc.replace('\\', '/')))
	ctx = _Ctx(root, os.path.dirname(path), conditionals, macros or {})
	ctx.config = config
	_parse_file(ctx, path)
	proj = ctx.proj
	proj.sources = _uniq([s for s in proj.sources if s.lower() not in ctx.removed])
	proj.protos = _uniq([s for s in proj.protos if s.lower() not in ctx.removed])
	proj.nuts = _uniq([s for s in proj.nuts if s.lower() not in ctx.removed])
	proj.projdir = ctx.rel(ctx.projdir)
	proj.libs = _uniq(proj.libs)
	proj.implibs = _uniq(proj.implibs)
	proj.macros = ctx.macros
	return proj
