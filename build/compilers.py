# $Id$

from os import environ, remove
from os.path import isfile
from shlex import split as shsplit
from subprocess import PIPE, STDOUT, Popen
import sys

def writeFile(path, lines):
	out = open(path, 'w')
	try:
		for line in lines:
			print >> out, line
	finally:
		out.close()

if environ['OSTYPE'] == 'msys':
	# The MSYS shell provides a Unix-like file system by translating paths on
	# the command line to Windows paths. Usually this is transparent, but not
	# for us since we call GCC without going through the shell.

	# Figure out the root directory of MSYS.
	proc = Popen(
		[ environ['SHELL'], '-c', '%s -c \'import sys ; print sys.argv[1]\' /'
			% sys.executable.replace('\\', '\\\\') ],
		stdin = None,
		stdout = PIPE,
		stderr = PIPE,
		)
	stdoutdata, stderrdata = proc.communicate()
	if stderrdata or proc.returncode:
		if stderrdata:
			print >> sys.stderr, 'Error determining MSYS root:', stderrdata
		if proc.returncode:
			print >> sys.stderr, 'Exit code %d' % proc.returncode
		raise IOError('Error determining MSYS root')
	msysRoot = stdoutdata.strip()

	# Figure out all mount points of MSYS.
	msysMounts = { '/': msysRoot + '/' }
	try:
		inp = open(msysRoot + '/etc/fstab')
		try:
			for line in inp:
				line = line.strip()
				if line and not line.startswith('#'):
					nativePath, mountPoint = (
						path.rstrip('/') + '/' for path in line.split()
						)
					msysMounts[mountPoint] = nativePath
		finally:
			inp.close()
	except IOError, ex:
		print >> sys.stderr, 'Failed to read MSYS fstab:', ex
	except ValueError, ex:
		print >> sys.stderr, 'Failed to parse MSYS fstab:', ex

	print msysMounts

	def fixMSYSPath(path):
		if path.startswith('/'):
			if len(path) == 2 or (len(path) > 2 and path[2] == '/'):
				# Support drive letters as top-level dirs.
				return '%s:/%s' % (path[1], path[3 : ])
			longestMatch = ''
			for mountPoint in msysMounts.iterkeys():
				if path.startswith(mountPoint):
					if len(mountPoint) > len(longestMatch):
						longestMatch = mountPoint
			return msysMounts[longestMatch] + path[len(longestMatch) : ]
		else:
			return path
	def fixFlags(flags):
		for i in xrange(len(flags)):
			flag = flags[i]
			if flag.startswith('-I'):
				yield '-I' + fixMSYSPath(flag[2 : ])
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
