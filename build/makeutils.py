# $Id$

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
	inp = open(filePath, 'r')
	try:
		for groups in filterLines(inp, regex):
			yield groups
	finally:
		inp.close()

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

def extractMakeVariables(filePath):
	'''Extract all variable definitions from the given Makefile.
	Returns a dictionary that maps each variable name to its value.
	'''
	makeVars = {}
	inp = open(filePath, 'r')
	try:
		for name, value in filterLines(
			joinContinuedLines(inp),
			r'[ ]*([A-Za-z0-9_]+)[ ]*:=(.*)'
			):
			makeVars[name] = value.strip()
	finally:
		inp.close()
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
