# $Id$
#
# Display probe results, so user can decide whether to start the build,
# or to change system configuration and rerun "configure".

from components import checkComponents, componentNames
from makeutils import extractMakeVariables, parseBool

import sys

libraries = (
	('libpng', 'PNG'),
	('libxml2', 'XML'),
	('OpenGL', 'GL'),
	('GLEW', 'GLEW'),
	('Jack', 'JACK'),
	('SDL', 'SDL'),
	('SDL_image', 'SDL_IMAGE'),
	('SDL_ttf', 'SDL_TTF'),
	('Tcl', 'TCL'),
	('zlib', 'ZLIB'),
	)

headers = (
	('libpng', 'PNG'),
	('libxml2', 'XML'),
	('OpenGL', ('GL', 'GL_GL')),
	('GLEW', ('GLEW', 'GL_GLEW')),
	('Jack', 'JACK'),
	('SDL', 'SDL'),
	('SDL_image', 'SDL_IMAGE'),
	('SDL_ttf', 'SDL_TTF'),
	('Tcl', 'TCL'),
	('zlib', 'ZLIB'),
	)

components = (
	('Emulation core', 'CORE'),
	('CassetteJack', 'JACK'),
	('SDLGL renderer', 'GL'),
	)

def iterProbeResults(probeMakePath):
	probeVars = extractMakeVariables(probeMakePath)
	customVars = extractMakeVariables('build/custom.mk')
	componentStatus = dict(zip(componentNames, checkComponents(probeVars)))
	maxLen = max(
		len(niceName)
		for niceName, _ in libraries + headers + components
		)

	yield ''
	if not parseBool(probeVars['COMPILER']):
		yield 'No working C++ compiler was found.'
		yield "Please install a C++ compiler, such as GCC's g++."
		yield 'If you have a C++ compiler installed and openMSX did not detect it, please set the environment variable OPENMSX_CXX to the name of your C++ compiler.'
		yield 'After you have corrected the situation, rerun "configure".'
		yield ''
	else:
		yield 'Found libraries:'
		disabledLibs = probeVars['DISABLED_LIBS'].split()
		formatStr = '  %-' + str(maxLen + 3) + 's %s'
		for niceName, varName in libraries:
			found = probeVars['HAVE_%s_LIB' % varName] or (
				'disabled' if varName in disabledLibs else 'no'
				)
			yield formatStr % (niceName + ':', found)
		yield ''

		yield 'Found headers:'
		disabledHeaders = probeVars['DISABLED_HEADERS'].split()
		for niceName, varNames in headers:
			if not hasattr(varNames, '__iter__'):
				varNames = (varNames, )
			for varName in varNames:
				if probeVars['HAVE_%s_H' % varName]:
					found = 'yes'
					break
			else:
				if any(varName in disabledHeaders for varName in varNames):
					found = 'disabled'
				else:
					found = 'no'
			yield formatStr % (niceName + ':', found)
		yield ''

		yield 'Components overview:'
		for niceName, varName in components:
			buildable = componentStatus[varName]
			yield formatStr % (niceName + ':', 'yes' if buildable else 'no')
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

if len(sys.argv) == 2:
	probeMakePath = sys.argv[1]
	for line in iterProbeResults(probeMakePath):
		print line
else:
	print >> sys.stderr, (
		'Usage: python probe-results.py PROBE_MAKE'
		)
	sys.exit(2)
