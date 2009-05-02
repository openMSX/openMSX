# $Id$

from msysutils import msysActive, msysPathToNative

from os import environ
from shlex import split as shsplit
from subprocess import PIPE, STDOUT, Popen

if msysActive():
	def fixArgs(args):
		for arg in args:
			if arg.startswith('-I') or arg.startswith('-L'):
				yield arg[ : 2] + msysPathToNative(arg[2 : ])
			elif arg.startswith('/'):
				yield msysPathToNative(arg)
			else:
				yield arg
else:
	def fixArgs(args):
		return iter(args)

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
					list(fixArgs(commandParts[1 : ] + flags))
					)
		else:
			raise ValueError(
				'No %s command specified in "%s"' % (cls.name, commandStr)
				)

	def __init__(self, env, executable, flags):
		self.__env = env
		self.__executable = executable
		self.__flags = flags

		mergedEnv = dict(environ)
		mergedEnv.update(env)
		self.__mergedEnv = mergedEnv

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
		commandLine = [ self.__executable ] + args + self.__flags
		try:
			proc = Popen(
				commandLine,
				bufsize = -1,
				env = self.__mergedEnv,
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
