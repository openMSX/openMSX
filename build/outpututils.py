# $Id$
# Various utility functions for generating output files and directories.

from os import makedirs
from os.path import dirname, isdir, isfile

def createDirFor(filePath):
	'''Creates an output directory for containing the given file path.
	Nothing happens if the directory already exsits.
	'''
	dirPath = dirname(filePath)
	if dirPath and not isdir(dirPath):
		makedirs(dirPath)

def rewriteIfChanged(path, lines):
	'''Writes the file with the given path if it does not exist yet or if its
	contents should change. The contents are given by the "lines" sequence.
	Returns True if the file was (re)written, False otherwise.
	'''
	newLines = [ line + '\n' for line in lines ]

	if isfile(path):
		inp = open(path, 'r')
		try:
			oldLines = inp.readlines()
		finally:
			inp.close()

		if newLines == oldLines:
			print 'Up to date: %s' % path
			return False
		else:
			print 'Updating %s...' % path
	else:
		print 'Creating %s...' % path
		createDirFor(path)

	out = open(path, 'w')
	try:
		out.writelines(newLines)
	finally:
		out.close()
	return True
