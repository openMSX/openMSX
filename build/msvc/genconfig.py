import os.path
import sys
import outpututils
import buildinfo2code
import components2code
import probe_header
import win_resource
import version2code

if len(sys.argv) != 4:
	print >> sys.stderr, "Usage: python genconfig.py platform configuration outputPath"
	sys.exit(2)
else:
	# One of { Win32, x64 }
	platform = sys.argv[1]
	
	# One of { Debug, Developer, Release }
	configuration = sys.argv[2]
	
	# The location in which to generate config files
	outputPath = sys.argv[3]
	
	buildPath = "build"
	msvcPath = os.path.join(buildPath, "msvc")
	probeMakePath = os.path.join(msvcPath, "probed_defs.mk")
	
	#
	# build-info.hh
	#
	buildInfoHeader = os.path.join(outputPath, "build-info.hh")
	targetPlatform = "mingw32"
	if platform == "Win32":
		targetCPU = "x86"
	elif platform == "x64":
		targetCPU = "x86_64"
	else:
		raise ValueError("Invalid platform: " + platform)
	flavour = configuration
	installShareDir = "/opt/openMSX/share" #not used on Windows, so whatever
	generator = buildinfo2code.iterBuildInfoHeader(targetPlatform, targetCPU, flavour, installShareDir)
	outpututils.rewriteIfChanged(buildInfoHeader, generator)
	
	#
	# components.hh
	#
	componentsHeader = os.path.join(outputPath, "components.hh")
	generator = components2code.iterComponentsHeader(probeMakePath)
	outpututils.rewriteIfChanged(componentsHeader, generator)

	#
	# probed_defs.hh
	#
	probedDefsHeader = os.path.join(outputPath, "probed_defs.hh")
	generator = probe_header.iterProbeHeader(probeMakePath)
	outpututils.rewriteIfChanged(probedDefsHeader, generator)

	#
	# resource-info.hh
	#
	resourceInfoHeader = os.path.join(outputPath, "resource-info.h")
	generator = win_resource.iterResourceHeader()
	outpututils.rewriteIfChanged(resourceInfoHeader, generator)

	#
	# version.ii
	#
	versionHeader = os.path.join(outputPath, "version.ii")
	generator = version2code.iterVersionInclude()
	outpututils.rewriteIfChanged(versionHeader, generator)
