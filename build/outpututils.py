# Various utility functions for generating output files and directories.

from io import open
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
	newLines = [line + '\n' for line in lines]

	if isfile(path):
		with open(path, 'r', encoding='utf-8') as inp:
			oldLines = inp.readlines()
		if newLines == oldLines:
			print('Up to date: %s' % path)
			return False
		else:
			print('Updating %s...' % path)
	else:
		print('Creating %s...' % path)
		createDirFor(path)

	with open(path, 'w', encoding='utf-8') as out:
		out.writelines(newLines)
	return True
