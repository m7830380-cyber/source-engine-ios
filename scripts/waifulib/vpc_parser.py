# It looks like shit, but I'm not particularly worried about such scripts.

import os
import re

_token_re = re.compile(r'\s*(&&|\|\||!|\(|\)|[a-zA-Z0-9_.]+)')

match_statement = re.compile(r'\[.*\]')

def compute_statement( defines, statement ):
	# Evaluates VPC conditionals like [ ($WIN32 || $OSXALL) && !$NO_STEAM ].
	# Precedence: ! > && > ||, parentheses supported.
	vars = set(d.split('=')[0] for d in defines)

	expr = re.sub(r'\[|\]|\$', '', statement)
	toks = []
	pos = 0
	while pos < len(expr):
		r = _token_re.match(expr, pos)
		if not r:
			if expr[pos:].strip() == '':
				break
			pos += 1 # skip unknown character instead of looping forever
			continue
		toks.append(r.group(1))
		pos = r.end()

	idx = [0]

	def peek():
		return toks[idx[0]] if idx[0] < len(toks) else None

	def take():
		t = peek()
		idx[0] += 1
		return t

	def atom():
		t = take()
		if t is None: return False
		if t == '!': return not atom()
		if t == '(':
			v = or_expr()
			if peek() == ')': take()
			return v
		if t == '1': return True
		if t == '0': return False
		return t in vars

	def and_expr():
		v = atom()
		while peek() == '&&':
			take()
			r = atom()
			v = v and r
		return v

	def or_expr():
		v = and_expr()
		while peek() == '||':
			take()
			r = and_expr()
			v = v or r
		return v

	if not toks:
		return True
	return or_expr()

def project_key(l):
	for k in l.keys():
		if '$Project' in k:
			return k

def fix_dos_path( path ):
	path = path.replace('\\', '/')
	p = path.split('/')

	filename = p[-1]
	find_path = '/'.join(p[0:len(p)-1])
	if find_path == '': find_path = './'
	else: find_path += '/'

	if not os.path.exists(find_path):
		return find_path+filename

	dirlist = os.listdir(find_path)
	for file in dirlist:
		if file == filename:
			return find_path+file
		elif file.lower() == filename.lower():
			return find_path+file
	return find_path+filename

def parse_vpcs( env ,vpcs, basedir ):
	back_path = os.path.abspath('.')
	os.chdir(env.SUBPROJECT_PATH[0])

	sources = []
	defines = []
	includes = []

	for vpc in vpcs:
		f=open(vpc, 'r').read().replace('\\\n', ';')

		re.sub(r'//.*', '', f)
		l = f.split('\n')

		iBrackets = 0

		next_br = False
		ret = {}
		cur_key = ''

		for i in l:
			if i == '': continue

			s = match_statement.search(i)
			if s and not compute_statement(env.DEFINES+defines, s.group(0)):
				continue

			if i.startswith('$') and iBrackets == 0:
				ret.update({i:[]})
				cur_key = i
				next_br = True
			elif i == '{':
				iBrackets += 1
				next_br = False
			elif i == '}':
				iBrackets -= 1
			elif iBrackets > 0:
				ret[cur_key].append(i)

			if next_br:
				next_br = False

		key = project_key(ret)
		l=ret[key]

		for i in l:
			if '-$File' in i and '.h"' not in i:
				for k in i.split(';'):
					k = k.replace('$SRCDIR', basedir)
					k = k.replace('$SRVSRCDIR', '.')
					k = k.replace('$GENERATED_PROTO_DIR', 'generated_proto')
					s = fix_dos_path(k.split('"')[1])

					for j in range(len(sources)):
						if sources[j] == s:
							del sources[j]
							break

			elif '$File' in i and '.h"' not in i:
				for j in i.split(';'):
					j = j.replace('$SRCDIR', basedir)
					j = j.replace('$SRVSRCDIR', '.')
					j = j.replace('$GENERATED_PROTO_DIR', 'generated_proto')
					s = fix_dos_path(j.split('"')[1])
					sources.append(s)

		for i in ret['$Configuration']:
			if '$PreprocessorDefinitions' in i:
				i = i.replace('$BASE', '')
				s = i.split('"')[1]
				s = re.split(';|,', s)
				for j in s:
					if j != '' and j not in defines:
						defines.append(j)
			if '$AdditionalIncludeDirectories' in i:
				i = i.replace('$BASE', '').replace('$SRCDIR', basedir).replace('$SRVSRCDIR', '.')
				s = i.split('"')[1]
				s = re.split(';|,', s)
				for j in s:
					j = j.replace('\\','/')
					if j != '' and j not in includes:
						includes.append(j)
	os.chdir(back_path)

	return {'defines':defines, 'includes':includes, 'sources': sources}
