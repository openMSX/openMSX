# $Id$
# Display probe results, so user can decide whether to start the build,
# or to change system configuration and rerun "configure".

from components import iterComponents
from makeutils import parseBool
from packages import getPackage

def iterProbeResults(probeVars, customVars, disabledHeaders, disabledLibs):
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
			'detect it, please set the environment variable OPENMSX_CXX to ' \
			'the name of your C++ compiler.'
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
			else:
				found = 'no'
			yield formatStr % (package.niceName + ':', found)
		yield ''

		yield 'Found headers:'
		for package in libraries:
			if set(package.iterHeaderMakeNames()).issubset(disabledHeaders):
				found = 'disabled'
			elif package.haveHeaders(probeVars):
				found = 'yes'
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
