# Generates configuration headers for VC++ builds

import sys
import os.path
import outpututils
import buildinfo2code
import components2code
import systemfuncs2code
import win_resource
import version2code

#
# platform: one of { Win32, x64 }
# configuration: one of { Debug, Developer, Release }
# outputPath: the location in which to generate config files
#
def genConfig(platform, configuration, outputPath):

	buildPath = 'build'
	msvcPath = os.path.join(buildPath, 'msvc')
	probeMakePath = os.path.join(msvcPath, 'probed_defs.mk')

	#
	# build-info.hh
	#
	buildInfoHeader = os.path.join(outputPath, 'build-info.hh')
	targetPlatform = 'mingw32'
	if platform == 'Win32':
		targetCPU = 'x86'
	elif platform == 'x64':
		targetCPU = 'x86_64'
	else:
		raise ValueError('Invalid platform: ' + platform)
	flavour = configuration
	installShareDir = '/opt/openMSX/share' #not used on Windows, so whatever
	generator = buildinfo2code.iterBuildInfoHeader(targetPlatform, targetCPU, flavour, installShareDir)
	outpututils.rewriteIfChanged(buildInfoHeader, generator)

	#
	# components.hh
	#
	componentsHeader = os.path.join(outputPath, 'components.hh')
	generator = components2code.iterComponentsHeader(probeMakePath)
	outpututils.rewriteIfChanged(componentsHeader, generator)

	#
	# systemfuncs.hh
	#

	systemFuncsHeader = os.path.join(outputPath, 'systemfuncs.hh')
	generator = systemfuncs2code.iterSystemFuncsHeader(systemfuncs2code.getSystemFuncsInfo())
	outpututils.rewriteIfChanged(systemFuncsHeader, generator)

	#
	# resource-info.hh
	#
	resourceInfoHeader = os.path.join(outputPath, 'resource-info.h')
	generator = win_resource.iterResourceHeader()
	outpututils.rewriteIfChanged(resourceInfoHeader, generator)

	#
	# version.ii
	#
	versionHeader = os.path.join(outputPath, 'version.ii')
	generator = version2code.iterVersionInclude()
	outpututils.rewriteIfChanged(versionHeader, generator)

if len(sys.argv) == 4:
	genConfig(sys.argv[1], sys.argv[2], sys.argv[3])
else:
	print('Usage: python3 genconfig.py platform configuration outputPath', file=sys.stderr)
	sys.exit(2)
