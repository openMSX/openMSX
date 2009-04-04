# $Id$

from msysutils import msysActive, msysPathToNative

from os import environ, remove
from os.path import isfile
from shlex import split as shsplit
from subprocess import PIPE, STDOUT, Popen

def writeFile(path, lines):
	out = open(path, 'w')
	try:
		for line in lines:
			print >> out, line
	finally:
		out.close()

if msysActive():
	def fixFlags(flags):
		for flag in flags:
			if flag.startswith('-I') or flag.startswith('-L'):
				yield flag[ : 2] + msysPathToNative(flag[2 : ])
			else:
				yield flag
else:
	def fixFlags(flags):
		return iter(flags)

class CompileCommand(object):

	@classmethod
	def fromLine(cls, compileCommandStr, compileFlagsStr):
		compileCmdParts = shsplit(compileCommandStr)
		compileFlags = shsplit(compileFlagsStr)
		compileEnv = {}
		while compileCmdParts:
			if '=' in compileCmdParts[0]:
				name, value = compileCmdParts[0].split('=', 1)
				del compileCmdParts[0]
				compileEnv[name] = value
			else:
				return cls(
					compileEnv,
					compileCmdParts[0],
					list(fixFlags(compileCmdParts[1 : ] + compileFlags))
					)
		else:
			raise ValueError(
				'No compiler specified in "%s"' % compileCommandStr
				)

	def __init__(self, env, executable, flags):
		mergedEnv = dict(environ)
		mergedEnv.update(env)
		self.__env = mergedEnv
		self.__executable = executable
		self.__flags = flags

	def __str__(self):
		return ' '.join(
			[ self.__executable ] + self.__flags + (
				[ '(%s)' % ' '.join(
					'%s=%s' % item
					for item in sorted(self.__env.iteritems())
					) ] if self.__env else []
				)
			)

	def compile(self, log, sourcePath, objectPath):
		try:
			proc = Popen(
				[ self.__executable ] + self.__flags +
					[ '-c', sourcePath, '-o', objectPath ],
				bufsize = -1,
				env = self.__env,
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

class LinkCommand(object):

	@classmethod
	def fromLine(cls, compileCommandStr, compileFlagsStr):
		# TODO: This is a copy of CompileCommand.fromLine().
		compileCmdParts = shsplit(compileCommandStr)
		compileFlags = shsplit(compileFlagsStr)
		compileEnv = {}
		while compileCmdParts:
			if '=' in compileCmdParts[0]:
				name, value = compileCmdParts[0].split('=', 1)
				del compileCmdParts[0]
				compileEnv[name] = value
			else:
				return cls(
					compileEnv,
					compileCmdParts[0],
					list(fixFlags(compileCmdParts[1 : ] + compileFlags))
					)
		else:
			raise ValueError(
				'No linker specified in "%s"' % compileCommandStr
				)

	def __init__(self, env, executable, flags):
		mergedEnv = dict(environ)
		mergedEnv.update(env)
		self.__env = mergedEnv
		self.__executable = executable
		self.__flags = flags

	def __str__(self):
		return ' '.join(
			[ self.__executable ] + self.__flags + (
				[ '(%s)' % ' '.join(
					'%s=%s' % item
					for item in sorted(self.__env.iteritems())
					) ] if self.__env else []
				)
			)

	def link(self, log, objectPaths, binaryPath):
		try:
			proc = Popen(
				[ self.__executable ] + self.__flags +
					objectPaths + [ '-o', binaryPath ],
				bufsize = -1,
				env = self.__env,
				stdin = None,
				stdout = PIPE,
				stderr = STDOUT,
				)
		except OSError, ex:
			print >> log, 'failed to execute linker: %s' % ex
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
			print >> log, 'return code from link command: %d' % (
				proc.returncode
				)
			return False

def tryCompile(log, compileCommand, sourcePath, lines):
	'''Write the program defined by "lines" to a text file specified
	by "path" and try to compile it.
	Returns True iff compilation succeeded.
	'''
	assert sourcePath.endswith('.cc')
	objectPath = sourcePath[ : -3] + '.o'
	writeFile(sourcePath, lines)
	try:
		return compileCommand.compile(log, sourcePath, objectPath)
	finally:
		remove(sourcePath)
		if isfile(objectPath):
			remove(objectPath)

def tryLink(log, compileCommand, linkCommand, sourcePath):
	assert sourcePath.endswith('.cc')
	objectPath = sourcePath[ : -3] + '.o'
	binaryPath = sourcePath[ : -3] + '.bin'
	def dummyProgram():
		# Try to link dummy program to the library.
		yield 'int main(int argc, char **argv) { return 0; }'
	writeFile(sourcePath, dummyProgram())
	try:
		compileOK = compileCommand.compile(log, sourcePath, objectPath)
		if not compileOK:
			print >> log, 'cannot test linking because compile failed'
			return False
		return linkCommand.link(log, [ objectPath ], binaryPath)
	finally:
		remove(sourcePath)
		if isfile(objectPath):
			remove(objectPath)
		if isfile(binaryPath):
			remove(binaryPath)
