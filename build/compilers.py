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

class _Command(object):
	name = None

	@classmethod
	def fromLine(cls, commandStr, flagsStr):
		commandParts = shsplit(commandStr)
		flags = shsplit(flagsStr)
		env = {}
		while commandParts:
			if '=' in commandParts[0]:
				name, value = commandParts[0].split('=', 1)
				del commandParts[0]
				env[name] = value
			else:
				return cls(
					env,
					commandParts[0],
					list(fixFlags(commandParts[1 : ] + flags))
					)
		else:
			raise ValueError(
				'No %s command specified in "%s"' % (cls.name, commandStr)
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

	def _run(self, log, args):
		commandLine = [ self.__executable ] + self.__flags + args
		try:
			proc = Popen(
				commandLine,
				bufsize = -1,
				env = self.__env,
				stdin = None,
				stdout = PIPE,
				stderr = STDOUT,
				)
		except OSError, ex:
			print >> log, 'failed to execute %s: %s' % (self.name, ex)
			return False
		stdoutdata, stderrdata = proc.communicate()
		if stdoutdata:
			log.write('%s command: %s\n' % (self.name, ' '.join(commandLine)))
			# pylint 0.18.0 somehow thinks stdoutdata is a list, not a string.
			# pylint: disable-msg=E1103
			stdoutdata = stdoutdata.replace('\r', '')
			log.write(stdoutdata)
			if not stdoutdata.endswith('\n'):
				log.write('\n')
		assert stderrdata is None, stderrdata
		if proc.returncode == 0:
			return True
		else:
			print >> log, 'return code from %s: %d' % (
				self.name, proc.returncode
				)
			return False

class CompileCommand(_Command):
	name = 'compiler'

	def compile(self, log, sourcePath, objectPath):
		return self._run(log, [ '-c', sourcePath, '-o', objectPath ])

class LinkCommand(_Command):
	name = 'linker'

	def link(self, log, objectPaths, binaryPath):
		return self._run(log, objectPaths + [ '-o', binaryPath ])

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
