# $Id$

from os import environ, remove
from os.path import isfile
from subprocess import PIPE, STDOUT, Popen

def writeFile(path, lines):
	out = open(path, 'w')
	try:
		for line in lines:
			print >> out, line
	finally:
		out.close()

def tryCompile(
	log, compileEnv, compileCommand, compileFlags, sourcePath, lines
	):
	'''Write the program defined by "lines" to a text file specified by "path"
	and try to compile it.
	Returns True iff compilation succeeded.
	'''
	mergedEnv = dict(environ)
	mergedEnv.update(compileEnv)

	assert sourcePath.endswith('.cc')
	objectPath = sourcePath[ : -3] + '.o'
	writeFile(sourcePath, lines)

	try:
		try:
			proc = Popen(
				[ compileCommand ] + compileFlags +
					[ '-c', sourcePath, '-o', objectPath ],
				bufsize = -1,
				env = mergedEnv,
				stdin = None,
				stdout = PIPE,
				stderr = STDOUT,
				)
		except OSError, ex:
			print >> log, 'failed to execute compiler: %s' % ex
			return False
		stdoutdata, stderrdata = proc.communicate()
		if stdoutdata:
			log.write(stdoutdata)
			if not stdoutdata.endswith('\n'): # pylint: disable-msg=E1103
				log.write('\n')
		assert stderrdata is None, stderrdata
		if proc.returncode == 0:
			return True
		else:
			print >> log, 'return code from compile command: %d' % (
				proc.returncode
				)
			return False
		return proc.returncode == 0
	finally:
		remove(sourcePath)
		if isfile(objectPath):
			remove(objectPath)
