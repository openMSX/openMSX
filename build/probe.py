# $Id$
# Replacement for autoconf.
# Performs some test compiles, to check for headers and functions.
# It does not execute anything it builds, making it friendly for cross compiles.

from compilers import CompileCommand, LinkCommand
from components import iterComponents
from makeutils import extractMakeVariables, parseBool
from outpututils import rewriteIfChanged
from packages import getPackage
from probe_defs import librariesByName, systemFunctions

from msysutils import msysActive
from os import environ, makedirs, remove
from os.path import isdir, isfile
from shlex import split as shsplit
from subprocess import PIPE, Popen
import sys

def resolve(log, expr):
	# TODO: Since for example "sdl-config" is used in more than one
	#       CFLAGS definition, it will be executed multiple times.
	try:
		return normalizeWhitespace(evaluateBackticks(log, expr))
	except IOError:
		# Executing a lib-config script is expected to fail if the
		# script is not installed.
		# TODO: Report this explicitly in the probe results table.
		return ''

def writeFile(path, lines):
	out = open(path, 'w')
	try:
		for line in lines:
			print >> out, line
	finally:
		out.close()

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
		# TODO: Use object file instead of dummy program.
		#       That way, object file can refer to symbol from the lib and
		#       we get more useful results from static libs.
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

def checkCompiler(log, compileCommand, outDir):
	'''Checks whether compiler can compile anything at all.
	Returns True iff the compiler works.
	'''
	def hello():
		# The most famous program.
		yield '#include <iostream>'
		yield 'int main(int argc, char** argv) {'
		yield '  std::cout << "Hello World!" << std::endl;'
		yield '  return 0;'
		yield '}'
	return tryCompile(log, compileCommand, outDir + '/hello.cc', hello())

def checkFunc(log, compileCommand, outDir, checkName, funcName, headers):
	'''Checks whether the given function is declared by the given headers.
	Returns True iff the function is declared.
	'''
	def takeFuncAddr():
		# Try to include the necessary headers and get the function address.
		for header in headers:
			yield '#include %s' % header
		yield 'void (*f)() = reinterpret_cast<void (*)()>(%s);' % funcName
	return tryCompile(
		log, compileCommand, outDir + '/' + checkName + '.cc', takeFuncAddr()
		)

def checkLib(log, compileCommand, linkCommand, outDir, makeName):
	'''Checks whether the given library can be linked against.
	Returns True iff the library is available.
	'''
	return tryLink(
		log, compileCommand, linkCommand, outDir + '/' + makeName + '.cc'
		)

def backtick(log, commandLine):
	'''Run a command and capture what it writes to stdout.
	If the command fails or writes something to stderr, that is logged.
	Returns the captured string, or None if the command failed.
	'''
	# TODO: This is a modified copy-paste from compilers._Command.
	commandParts = shsplit(commandLine)
	env = dict(environ)
	while commandParts:
		if '=' in commandParts[0]:
			name, value = commandParts[0].split('=', 1)
			del commandParts[0]
			env[name] = value
		else:
			break
	else:
		raise ValueError(
			'No command specified in "%s"' % commandLine
			)

	if msysActive():
		commandParts = [ environ['SHELL'], '-c', shjoin(commandParts) ]

	try:
		proc = Popen(
			commandParts, bufsize = -1, env = env,
			stdin = None, stdout = PIPE, stderr = PIPE,
			)
	except OSError, ex:
		print >> log, 'Failed to execute "%s": %s' % (commandLine, ex)
		return None
	stdoutdata, stderrdata = proc.communicate()
	if stderrdata:
		severity = 'warning' if proc.returncode == 0 else 'error'
		log.write('%s executing "%s"\n' % (severity.capitalize(), commandLine))
		# pylint 0.18.0 somehow thinks stdoutdata is a list, not a string.
		# pylint: disable-msg=E1103
		stderrdata = stderrdata.replace('\r', '')
		log.write(stderrdata)
		if not stderrdata.endswith('\n'):
			log.write('\n')
	if proc.returncode == 0:
		return stdoutdata
	else:
		print >> log, 'Execution failed with exit code %d' % proc.returncode
		return None

def evaluateBackticks(log, expression):
	parts = []
	index = 0
	while True:
		start = expression.find('`', index)
		if start == -1:
			parts.append(expression[index : ])
			break
		end = expression.find('`', start + 1)
		if end == -1:
			raise ValueError('Unmatched backtick: %s' % expression)
		parts.append(expression[index : start])
		command = expression[start + 1 : end].strip()
		result = backtick(log, command)
		if result is None:
			raise IOError('Backtick evaluation failed; see log')
		parts.append(result)
		index = end + 1
	return ''.join(parts)

def shjoin(parts):
	def escape(part):
		return ''.join(
			'\\' + ch if ch in '\\ \'"$()[]' else ch
			for ch in part
			)
	return ' '.join(escape(part) for part in parts)

def normalizeWhitespace(expression):
	return shjoin(shsplit(expression))

class TargetSystem(object):

	def __init__(
		self,
		log, compileCommandStr, outDir, platform, probeVars, customVars,
		disabledLibraries
		):
		'''Create empty log and result files.
		'''
		self.log = log
		self.compileCommandStr = compileCommandStr
		self.outDir = outDir
		self.platform = platform
		self.probeVars = probeVars
		self.customVars = customVars
		self.disabledLibraries = disabledLibraries
		self.outMakePath = outDir + '/probed_defs.mk'
		self.outHeaderPath = outDir + '/probed_defs.hh'
		self.outVars = {}

	def checkAll(self):
		'''Run all probes.
		'''
		self.hello()
		for func in systemFunctions:
			self.checkFunc(func)
		for library in sorted(librariesByName.iterkeys()):
			if library in self.disabledLibraries:
				self.disabledLibrary(library)
			else:
				self.checkLibrary(library)

	def writeAll(self):
		def iterVars():
			yield '# Automatically generated by build system.'
			yield '# Non-empty value means found, empty means not found.'
			for item in sorted(self.outVars.iteritems()):
				yield '%s:=%s' % item
		rewriteIfChanged(self.outMakePath, iterVars())

		def iterProbeHeader():
			yield '// Automatically generated by build system.'
			for makeName in sorted(
				'HAVE_%s' % func.getMakeName() for func in systemFunctions
				):
				if self.outVars[makeName]:
					yield '#define %s 1' % makeName
				else:
					yield '// #undef %s' % makeName
		rewriteIfChanged(self.outHeaderPath, iterProbeHeader())

	def printResults(self):
		for line in iterProbeResults(
			self.outVars, self.customVars, self.disabledLibraries
			):
			print line

	def everything(self):
		self.checkAll()
		self.writeAll()
		self.printResults()

	def hello(self):
		'''Check compiler with the most famous program.
		'''
		compileCommand = CompileCommand.fromLine(self.compileCommandStr, '')
		ok = checkCompiler(self.log, compileCommand, self.outDir)
		print >> self.log, 'Compiler %s: %s' % (
			'works' if ok else 'broken',
			compileCommand
			)
		self.outVars['COMPILER'] = str(ok).lower()

	def checkFunc(self, func):
		'''Probe for function.
		'''
		compileCommand = CompileCommand.fromLine(self.compileCommandStr, '')
		ok = checkFunc(
			self.log, compileCommand, self.outDir,
			func.name, func.getFunctionName(), func.iterHeaders(self.platform)
			)
		print >> self.log, '%s function: %s' % (
			'Found' if ok else 'Missing',
			func.getFunctionName()
			)
		self.outVars['HAVE_%s' % func.getMakeName()] = 'true' if ok else ''

	def checkHeader(self, makeName, compileCommand):
		funcName = self.probeVars['%s_FUNCTION' % makeName]
		header = self.probeVars['%s_HEADER' % makeName]
		ok = checkFunc(
			self.log, compileCommand, self.outDir, makeName, funcName,
			[ header ]
			)
		print >> self.log, '%s header: %s' % (
			'Found' if ok else 'Missing',
			makeName
			)
		self.outVars['HAVE_%s_H' % makeName] = 'true' if ok else ''

	def checkLib(self, makeName, compileCommand, linkCommand):
		ok = checkLib(
			self.log, compileCommand, linkCommand, self.outDir, makeName
			)
		print >> self.log, '%s lib: %s' % (
			'Found' if ok else 'Missing',
			makeName
			)
		if ok:
			result = resolve(self.log, self.probeVars['%s_RESULT' % makeName])
		else:
			result = ''
		self.outVars['HAVE_%s_LIB' % makeName] = result

	def checkLibrary(self, makeName):
		cflags = resolve(self.log, self.probeVars['%s_CFLAGS' % makeName])
		ldflags = resolve(self.log, self.probeVars['%s_LDFLAGS' % makeName])
		compileCommand = CompileCommand.fromLine(self.compileCommandStr, cflags)
		linkCommand = LinkCommand.fromLine(self.compileCommandStr, ldflags)
		self.outVars['%s_CFLAGS' % makeName] = cflags
		self.outVars['%s_LDFLAGS' % makeName] = ldflags
		self.checkHeader(makeName, compileCommand)
		self.checkLib(makeName, compileCommand, linkCommand)

	def disabledLibrary(self, library):
		print >> self.log, 'Disabled library: %s' % library
		self.outVars['HAVE_%s_H' % library] = ''
		self.outVars['HAVE_%s_LIB' % library] = ''

def iterProbeResults(probeVars, customVars, disabledLibs):
	'''Present probe results, so user can decide whether to start the build,
	or to change system configuration and rerun "configure".
	'''
	componentStatus = dict(
		(component.makeName, component.canBuild(probeVars))
		for component in iterComponents()
		)
	libraries = sorted(
		(	getPackage(packageName)
			for component in iterComponents()
			for packageName in component.dependsOn
			),
		key = lambda package: package.niceName.lower()
		)

	yield ''
	if not parseBool(probeVars['COMPILER']):
		yield 'No working C++ compiler was found.'
		yield "Please install a C++ compiler, such as GCC's g++."
		yield 'If you have a C++ compiler installed and openMSX did not ' \
			'detect it, please set the environment variable CXX to the name ' \
			'of your C++ compiler.'
		yield 'After you have corrected the situation, rerun "configure".'
		yield ''
	else:
		# Compute how wide the first column should be.
		def iterNiceNames():
			for package in libraries:
				yield package.niceName
			for component in iterComponents():
				yield component.niceName
		maxLen = max(len(niceName) for niceName in iterNiceNames())
		formatStr = '  %-' + str(maxLen + 3) + 's %s'

		yield 'Found libraries:'
		for package in libraries:
			if package.getMakeName() in disabledLibs:
				found = 'disabled'
			elif package.haveLibrary(probeVars):
				found = probeVars['HAVE_%s_LIB' % package.getMakeName()]
			elif package.haveHeaders(probeVars):
				# Dependency resolution of a typical distro will not allow
				# this situation. Most likely we got the link flags wrong.
				found = 'headers found, link test failed'
			else:
				found = 'no'
			yield formatStr % (package.niceName + ':', found)
		yield ''

		yield 'Components overview:'
		for component in iterComponents():
			yield formatStr % (
				component.niceName + ':',
				'yes' if componentStatus[component.makeName] else 'no'
				)
		yield ''

		yield 'Customisable options:'
		yield formatStr % ('Install to', customVars['INSTALL_BASE'])
		yield '  (you can edit these in build/custom.mk)'
		yield ''

		if all(componentStatus.itervalues()):
			yield 'All required and optional components can be built.'
		elif componentStatus['CORE']:
			yield 'If you are satisfied with the probe results, ' \
				'run "make" to start the build.'
			yield 'Otherwise, install some libraries and headers ' \
				'and rerun "configure".'
		else:
			yield 'Please install missing libraries and headers ' \
				'and rerun "configure".'
		yield ''

def main(compileCommandStr, outDir, platform, linkMode, thirdPartyInstall):
	if not isdir(outDir):
		makedirs(outDir)
	log = open(outDir + '/probe.log', 'w')
	print 'Probing target system...'
	print >> log, 'Probing system:'
	try:
		customVars = extractMakeVariables('build/custom.mk')
		disabledLibraries = set(customVars['DISABLED_LIBRARIES'].split())
		if linkMode.startswith('3RD_'):
			# Disable Jack: The CassetteJack feature is not useful for most end
			# users, so do not include Jack in the binary distribution of
			# openMSX.
			disabledLibraries.add('JACK')

		distroRoot = thirdPartyInstall or None
		probeVars = {}
		for name, library in sorted(librariesByName.iteritems()):
			if name in disabledLibraries:
				continue
			probeVars['%s_HEADER' % name] = library.getHeader(platform)
			probeVars['%s_FUNCTION' % name] = library.function
			probeVars['%s_CFLAGS' % name] = \
				library.getCompileFlags(platform, linkMode, distroRoot)
			probeVars['%s_LDFLAGS' % name] = \
				library.getLinkFlags(platform, linkMode, distroRoot)
			probeVars['%s_RESULT' % name] = \
				library.getResult(platform, linkMode, distroRoot)

		TargetSystem(
			log, compileCommandStr, outDir, platform, probeVars, customVars,
			disabledLibraries
			).everything()
	finally:
		log.close()

if __name__ == '__main__':
	if len(sys.argv) == 6:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ve:
			print >> sys.stderr, ve
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python probe.py '
			'COMPILE OUTDIR OPENMSX_TARGET_OS LINK_MODE 3RDPARTY_INSTALL_DIR'
			)
		sys.exit(2)
