from io import open
from os.path import isdir
import re

def filterLines(lines, regex):
	'''Filters each line of the given line iterator using the given regular
	expression string. For each match, a tuple containing the text matching
	each capture group from the regular expression is yielded.
	'''
	matcher = re.compile(regex)
	for line in lines:
		if line.endswith('\n'):
			line = line[ : -1]
		match = matcher.match(line)
		if match:
			yield match.groups()

def filterFile(filePath, regex):
	'''Filters each line of the given text file using the given regular
	expression string. For each match, a tuple containing the text matching
	each capture group from the regular expression is yielded.
	'''
	with open(filePath, 'r', encoding='utf-8') as inp:
		for groups in filterLines(inp, regex):
			yield groups

def joinContinuedLines(lines):
	'''Iterates through the given lines, replacing lines that are continued
	using a trailing backslash with a single line.
	'''
	buf = ''
	for line in lines:
		if line.endswith('\\\n'):
			buf += line[ : -2]
		elif line.endswith('\\'):
			buf += line[ : -1]
		else:
			yield buf + line
			buf = ''
	if buf:
		raise ValueError('Continuation on last line')

_reEval = re.compile(r'(\$\(|\))')
def evalMakeExpr(expr, makeVars):
	'''Evaluates variable references in an expression.
	Raises ValueError if there is a syntax error in the expression.
	Raises KeyError if the expression references a non-existing variable.
	'''
	stack = [ [] ]
	for part in _reEval.split(expr):
		if part == '$(':
			stack.append([])
		elif part == ')' and len(stack) != 1:
			name = ''.join(stack.pop())
			if name.startswith('addprefix '):
				prefix, args = name[len('addprefix') : ].split(',')
				prefix = prefix.strip()
				value = ' '.join(prefix + arg for arg in args.split())
			elif name.startswith('addsuffix '):
				suffix, args = name[len('addsuffix') : ].split(',')
				suffix = suffix.strip()
				value = ' '.join(arg + suffix for arg in args.split())
			elif name.startswith('shell '):
				# Unsupported; assume result is never used.
				value = '?'
			elif name.isdigit():
				# This is a function argument; assume the evaluated result is
				# never used.
				value = '?'
			else:
				value = makeVars[name]
			stack[-1].append(value)
		else:
			stack[-1].append(part)
	if len(stack) != 1:
		raise ValueError('Open without close in "%s"' % expr)
	return ''.join(stack.pop())

def extractMakeVariables(filePath, makeVars = None):
	'''Extract all variable definitions from the given Makefile.
	The optional makeVars argument is a dictionary containing the already
	defined variables. These variables will be included in the output; the
	given dictionary is not modified.
	Returns a dictionary that maps each variable name to its value.
	'''
	makeVars = {} if makeVars is None else dict(makeVars)
	with open(filePath, 'r', encoding='utf-8') as inp:
		for name, assign, value in filterLines(
			joinContinuedLines(inp),
			r'[ ]*([A-Za-z0-9_]+)[ ]*([+:]?=)(.*)'
			):
			if assign == '=':
				makeVars[name] = value.strip()
			elif assign == ':=':
				makeVars[name] = evalMakeExpr(value, makeVars).strip()
			elif assign == '+=':
				# Note: Make will or will not evaluate the added expression
				#       depending on how the variable was originally defined,
				#       but we don't store that information.
				makeVars[name] = makeVars[name] + ' ' + value.strip()
			else:
				assert False, assign
	return makeVars

def parseBool(valueStr):
	'''Parses a string containing a boolean value.
	Accepted values are "true" and "false"; anything else raises ValueError.
	'''
	if valueStr == 'true':
		return True
	elif valueStr == 'false':
		return False
	else:
		raise ValueError('Invalid boolean "%s"' % valueStr)
