# $Id$
# Check the existence of a certain function in the standard library.

from os import environ, remove
from os.path import isfile
from subprocess import PIPE, STDOUT, Popen
import sys

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

def checkFunc(
	log, compileEnv, compileCommand, compileFlags, outDir,
	makeName, funcName, headers
	):
	'''Checks whether the given function is declared by the given headers.
	Returns True iff the function is declared.
	'''
	def takeFuncAddr():
		# Try to include the necessary headers and get the function address.
		for header in headers:
			yield '#include %s' % header
		yield 'void (*f)() = reinterpret_cast<void (*)()>(%s);' % funcName
	return tryCompile(
		log, compileEnv, compileCommand, compileFlags,
		outDir + '/' + makeName + '.cc', takeFuncAddr()
		)

def main(
	compileCommandStr, compileFlagsStr, outDir, logPath, makePath,
	makeName, funcName, headerStr
	):
	log = open(logPath, 'a')
	try:
		compileCommand = compileCommandStr.split()
		compileFlags = compileFlagsStr.split()
		compileEnv = {}
		while compileCommand:
			if '=' in compileCommand[0]:
				name, value = compileCommand[0].split('=', 1)
				del compileCommand[0]
				compileEnv[name] = value
			else:
				compileFlags = compileCommand[1 : ] + compileFlags
				ok = checkFunc(
					log, compileEnv, compileCommand[0], compileFlags, outDir,
					makeName, funcName, headerStr.split()
					)
				print >> log, '%s function: %s' % (
					'Found' if ok else 'Missing',
					makeName
					)
				break
		else:
			ok = False
			print >> log, 'No compiler specified in "%s"' % compileCommandStr

	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'HAVE_%s:=%s' % (makeName,'true' if ok else '')
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 9:
		main(*sys.argv[1 : ])
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_function.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE MAKENAME FUNCNAME HEADERS'
			)
		sys.exit(2)
