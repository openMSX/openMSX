# $Id$

from os import environ, remove
from os.path import isfile
from platform import system
from shlex import split as shsplit
from subprocess import PIPE, STDOUT, Popen

def writeFile(path, lines):
	out = open(path, 'w')
	try:
		for line in lines:
			print >> out, line
	finally:
		out.close()

if system().lower() == 'windows':
	# TODO: This should only be done for MinGW, not for all Windows builds.
	#       But right now GCC is the only Windows compiler we can drive
	#       directly anyway (MSVC++ is driven through msbuild).
	def fixMinGWPath(path):
		if len(path) >= 2 and path[0] == '/' and (
			len(path) == 2 or path[2] == '/'
			):
			return '%s:/%s' % (path[1], path[3 : ])
		else:
			return path
	def fixFlags(flags):
		for i in xrange(len(flags)):
			flag = flags[i]
			if flag.startswith('-I'):
				yield '-I' + fixMinGWPath(flag[2 : ])
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
		self.__env = env
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

	def tryCompile(self, log, sourcePath, lines):
		'''Write the program defined by "lines" to a text file specified
		by "path" and try to compile it.
		Returns True iff compilation succeeded.
		'''
		mergedEnv = dict(environ)
		mergedEnv.update(self.__env)

		assert sourcePath.endswith('.cc')
		objectPath = sourcePath[ : -3] + '.o'
		writeFile(sourcePath, lines)

		try:
			try:
				proc = Popen(
					[ self.__executable ] + self.__flags +
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
