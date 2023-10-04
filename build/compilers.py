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

	@classmethod
	def fromLine(cls, commandStr, flagsStr):
		commandParts = shsplit(commandStr)
		flags = shsplit(flagsStr)
		env = {'LC_ALL': 'C.UTF-8'}
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
			raise ValueError('No command specified in "%s"' % commandStr)

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
					for item in sorted(self.__env.items())
					) ] if self.__env else []
				)
			)

	def _run(self, log, name, args, inputSeq, captureOutput):
		commandLine = [ self.__executable ] + args + self.__flags
		try:
			proc = Popen(
				commandLine,
				bufsize = -1,
				env = self.__mergedEnv,
				stdin = None if inputSeq is None else PIPE,
				stdout = PIPE,
				stderr = PIPE if captureOutput else STDOUT,
				)
		except OSError as ex:
			print('failed to execute %s: %s' % (name, ex), file=log)
			return None if captureOutput else False
		inputText = None if inputSeq is None \
				else ('\n'.join(inputSeq) + '\n').encode('utf-8')
		stdoutdata, stderrdata = proc.communicate(inputText)
		stdouttext = stdoutdata.decode('utf-8', 'replace')
		if captureOutput:
			assert stderrdata is not None
			messages = stderrdata.decode('utf-8', 'replace')
		else:
			assert stderrdata is None
			messages = stdouttext
		if messages:
			log.write('%s command: %s\n' % (name, ' '.join(commandLine)))
			if inputText is not None:
				log.write('input:\n')
				log.write(str(inputText))
				if not inputText.endswith(b'\n'):
					log.write('\n')
				log.write('end input.\n')
			# pylint 0.18.0 somehow thinks 'messages' is a list, not a string.
			# pylint: disable-msg=E1103
			messages = messages.replace('\r', '')
			log.write(messages)
			if not messages.endswith('\n'):
				log.write('\n')
		if proc.returncode == 0:
			return stdouttext if captureOutput else True
		else:
			print('return code from %s: %d' % (name, proc.returncode), file=log)
			return None if captureOutput else False

class CompileCommand(_Command):
	__expandSignature = 'EXPAND_MACRO_'

	def compile(self, log, sourcePath, objectPath):
		return self._run(
			log, 'compiler', [ '-c', sourcePath, '-o', objectPath ], None, False
			)

	def expand(self, log, headers, *keys):
		signature = self.__expandSignature
		def iterLines():
			for header in headers:
				yield '#include %s' % header
			for key in keys:
				yield '%s%s %s' % (signature, key, key)
		output = self._run(
			log, 'preprocessor', [ '-E', '-' ], iterLines(), True
			)
		if output is None:
			if len(keys) == 1:
				return None
			else:
				return (None, ) * len(keys)
		else:
			expanded = {}
			prevKey = None
			for line in output.split('\n'):
				line = line.strip()
				if not line or line.startswith('#'):
					continue
				if line.startswith(signature):
					prevKey = None
					keyValueStr = line[len(signature) : ]
					try:
						key, value = keyValueStr.split(None, 1)
					except ValueError:
						key, value = keyValueStr, ''
					if key not in keys:
						log.write(
							'Ignoring macro expand signature match on '
							'non-requested macro "%s"\n' % key
							)
						continue
					elif value == '':
						# GCC5 puts value on separate line.
						prevKey = key
						continue
				elif prevKey is not None:
					key = prevKey
					value = line
					prevKey = None
				else:
					continue
				if value != key:
					expanded[key] = value
			if len(keys) == 1:
				return expanded.get(keys[0])
			else:
				return tuple(expanded.get(key) for key in keys)

class LinkCommand(_Command):

	def link(self, log, objectPaths, binaryPath):
		return self._run(
			log, 'linker', objectPaths + [ '-o', binaryPath ], None, False
			)
