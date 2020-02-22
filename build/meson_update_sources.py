#!/usr/bin/env python3

from os import walk

from outpututils import rewriteIfChanged


def scanSources(baseDir):
	files = []
	dirs = []
	for dirPath, dirNames, fileNames in walk(baseDir):
		assert dirPath.startswith(baseDir)
		prefix = dirPath[len(baseDir):]
		if prefix:
			if not (prefix == 'unittest' or prefix.endswith('__pycache__')):
				dirs.append(prefix)
			prefix += '/'
		else:
			dirs.append('.')
		files += (
			prefix + name
			for name in fileNames
			if name.endswith('.cc')
			)
	return files, dirs

def mesonSources():
	files, dirs = scanSources('src/')
	testSources = []
	yield "sources = files("
	for name in sorted(files):
		if name.startswith('unittest/'):
			testSources.append(name)
		elif not (name == 'main.cc'
				or name.endswith('Test.cc')
				or name.endswith('_test.cc')
				):
			yield "    '%s'," % name
	yield "    )"
	yield ""
	yield "main_sources = files("
	yield "    'main.cc',"
	yield "    )"
	yield ""
	yield "test_sources = files("
	for name in testSources:
		yield "    '%s'," % name
	yield "    )"
	yield ""
	yield "incdirs = include_directories("
	for name in dirs:
		yield "    '%s'," % name
	yield "    )"

if __name__ == '__main__':
	rewriteIfChanged('src/meson.build', mesonSources())
