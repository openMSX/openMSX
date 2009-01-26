# $Id$

import re

def filterFile(filePath, regex):
	'''Filters each line of the given text file using the given regular
	expression string. For each match, a tuple containing the text matching
	each capture group from the regular expression is yielded.
	'''
	matcher = re.compile(regex)
	inp = open(filePath, 'r')
	try:
		for line in inp:
			if line.endswith('\n'):
				line = line[ : -1]
			match = matcher.match(line)
			if match:
				yield match.groups()
	finally:
		inp.close()

def extractMakeVariables(filePath):
	'''Extract all variable definitions from the given Makefile.
	Returns a dictionary that maps each variable name to its value.
	'''
	makeVars = {}
	for name, value in filterFile(
		filePath, r'[ ]*([A-Za-z0-9_]+)[ ]*:=[ ]*([^ ]*)[ ]*'
		):
		makeVars[name] = value
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
