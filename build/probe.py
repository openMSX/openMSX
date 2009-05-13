# $Id$
# Replacement for autoconf.
# Performs some test compiles, to check for headers and functions.
# It does not execute anything it builds, making it friendly for cross compiles.

from compilers import CompileCommand, LinkCommand
from components import EmulationCore, GLRenderer, iterComponents
from makeutils import extractMakeVariables, parseBool
from outpututils import rewriteIfChanged
from packages import getPackage
from probe_defs import librariesByName, systemFunctions

from msysutils import msysActive
from os import environ, makedirs, remove
from os.path import isdir, isfile, pathsep
from shlex import split as shsplit
from subprocess import PIPE, Popen
import sys

def requiredLibrariesFor(components):
	'''Compute the library packages required to build the given components.
	Only the direct dependencies from openMSX are included, not dependencies
	between libraries.
	Returns a set of Make names.
	'''
	return set(
		makeName
		for comp in components
		for makeName in comp.dependsOn
		)

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
		log, compileCommandStr, outDir, platform, linkStatic, distroRoot,
		requiredComponents, desiredComponents
		):
		'''Create empty log and result files.
		'''
		self.log = log
		self.compileCommandStr = compileCommandStr
		self.outDir = outDir
		self.platform = platform
		self.linkStatic = linkStatic
		self.distroRoot = distroRoot
		self.requiredComponents = requiredComponents
		self.desiredComponents = desiredComponents
		self.outMakePath = outDir + '/probed_defs.mk'
		self.outHeaderPath = outDir + '/probed_defs.hh'
		self.outVars = {}

	def checkAll(self):
		'''Run all probes.
		'''
		self.hello()
		for func in systemFunctions:
			self.checkFunc(func)
		for library in sorted(requiredLibrariesFor(self.desiredComponents)):
			self.checkLibrary(library)

	def writeAll(self):
		def iterVars():
			yield '# Automatically generated by build system.'
			yield '# Non-empty value means found, empty means not found.'
			for library in sorted(requiredLibrariesFor(self.desiredComponents)):
				for name in (
					'HAVE_%s_H' % library,
					'HAVE_%s_LIB' % library,
					'%s_CFLAGS' % library,
					'%s_LDFLAGS' % library,
					):
					yield '%s:=%s' % (name, self.outVars[name])
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
			self.outVars, self.requiredComponents, self.desiredComponents
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

	def checkLibrary(self, makeName):
		library = librariesByName[makeName]
		cflags = resolve(
			self.log,
			library.getCompileFlags(
				self.platform, self.linkStatic, self.distroRoot
				)
			)
		ldflags = resolve(
			self.log,
			library.getLinkFlags(
				self.platform, self.linkStatic, self.distroRoot
				)
			)
		compileCommand = CompileCommand.fromLine(self.compileCommandStr, cflags)
		linkCommand = LinkCommand.fromLine(self.compileCommandStr, ldflags)
		self.outVars['%s_CFLAGS' % makeName] = cflags
		self.outVars['%s_LDFLAGS' % makeName] = ldflags

		sourcePath = self.outDir + '/' + makeName + '.cc'
		objectPath = self.outDir + '/' + makeName + '.o'
		binaryPath = self.outDir + '/' + makeName + '.bin'

		funcName = library.function
		headers = library.getHeaders(self.platform)
		def takeFuncAddr():
			# Try to include the necessary headers and get the function address.
			for header in headers:
				yield '#include %s' % header
			yield 'void (*f)() = reinterpret_cast<void (*)()>(%s);' % funcName
			yield 'int main(int argc, char** argv) {'
			yield '  return 0;'
			yield '}'
		writeFile(sourcePath, takeFuncAddr())
		try:
			compileOK = compileCommand.compile(self.log, sourcePath, objectPath)
			print >> self.log, '%s: %s header' % (
				makeName,
				'Found' if compileOK else 'Missing'
				)
			if compileOK:
				linkOK = linkCommand.link(self.log, [ objectPath ], binaryPath)
				print >> self.log, '%s: %s lib' % (
					makeName,
					'Found' if linkOK else 'Missing'
					)
			else:
				linkOK = False
				print >> self.log, (
					'%s: Cannot test linking because compile failed'
					% makeName
					)
		finally:
			remove(sourcePath)
			if isfile(objectPath):
				remove(objectPath)
			if isfile(binaryPath):
				remove(binaryPath)

		self.outVars['HAVE_%s_H' % makeName] = 'true' if compileOK else ''
		self.outVars['HAVE_%s_LIB' % makeName] = 'true' if linkOK else ''
		if linkOK:
			versionGet = library.getVersion(
				self.platform, self.linkStatic, self.distroRoot
				)
			if callable(versionGet):
				version = versionGet(compileCommand, self.log)
			else:
				version = resolve(self.log, versionGet)
			if version is None:
				version = 'error'
			self.outVars['VERSION_%s' % makeName] = version

def iterProbeResults(probeVars, requiredComponents, desiredComponents):
	'''Present probe results, so user can decide whether to start the build,
	or to change system configuration and rerun "configure".
	'''
	buildableComponents = set(
		comp for comp in desiredComponents if comp.canBuild(probeVars)
		)
	packages = sorted(
		( getPackage(makeName)
		  for makeName in requiredLibrariesFor(desiredComponents) ),
		key = lambda package: package.niceName.lower()
		)
	customVars = extractMakeVariables('build/custom.mk')

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
			for package in packages:
				yield package.niceName
			for component in iterComponents():
				yield component.niceName
		maxLen = max(len(niceName) for niceName in iterNiceNames())
		formatStr = '  %-' + str(maxLen + 3) + 's %s'

		yield 'Found libraries:'
		for package in packages:
			makeName = package.getMakeName()
			if probeVars['HAVE_%s_LIB' % makeName]:
				found = 'version %s' % probeVars['VERSION_%s' % makeName]
			elif probeVars['HAVE_%s_H' % makeName]:
				# Dependency resolution of a typical distro will not allow
				# this situation. Most likely we got the link flags wrong.
				found = 'headers found, link test failed'
			else:
				found = 'no'
			yield formatStr % (package.niceName + ':', found)
		yield ''

		yield 'Components overview:'
		for component in iterComponents():
			if component in desiredComponents:
				status = 'yes' if component in buildableComponents else 'no'
			else:
				status = 'disabled'
			yield formatStr % (component.niceName + ':', status)
		yield ''

		yield 'Customisable options:'
		yield formatStr % ('Install to', customVars['INSTALL_BASE'])
		yield '  (you can edit these in build/custom.mk)'
		yield ''

		if buildableComponents == desiredComponents:
			yield 'All required and optional components can be built.'
		elif requiredComponents.issubset(buildableComponents):
			yield 'If you are satisfied with the probe results, ' \
				'run "make" to start the build.'
			yield 'Otherwise, install some libraries and headers ' \
				'and rerun "configure".'
		else:
			yield 'Please install missing libraries and headers ' \
				'and rerun "configure".'
		yield ''

def main(compileCommandStr, outDir, platform, linkMode, thirdPartyInstall):
	assert linkMode in ('SYS_DYN', '3RD_STA')
	linkStatic = (linkMode == '3RD_STA')

	if linkStatic:
		# The CassetteJack feature is not useful for most end users and it is
		# the only component that requires the Jack library.
		requiredComponents = set((EmulationCore, GLRenderer))
		optionalComponents = set()
	else:
		requiredComponents = set((EmulationCore, ))
		optionalComponents = set(iterComponents()) - requiredComponents
	desiredComponents = requiredComponents | optionalComponents

	if not isdir(outDir):
		makedirs(outDir)
	log = open(outDir + '/probe.log', 'w')
	print 'Probing target system...'
	print >> log, 'Probing system:'
	try:
		distroRoot = thirdPartyInstall or None
		if platform == 'darwin' and distroRoot is None:
			for searchPath in environ.get('PATH', '').split(pathsep):
				if searchPath == '/opt/local/bin':
					print 'Using libraries from MacPorts.'
					distroRoot = '/opt/local'
					break
				elif searchPath == '/sw/bin':
					print 'Using libraries from Fink.'
					distroRoot = '/sw'
					break
			else:
				distroRoot = '/usr/locql'

		TargetSystem(
			log, compileCommandStr, outDir, platform, linkStatic, distroRoot,
			requiredComponents, desiredComponents
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
