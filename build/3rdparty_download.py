# $Id$

from components import requiredLibrariesFor
from configurations import getConfiguration
from download import downloadURL
from extract import TopLevelDirRenamer, extract
from libraries import librariesByName
from packages import getPackage
from patch import Diff, patch

from os import makedirs
from os.path import isdir, isfile, join as joinpath
from shutil import rmtree
import sys

# TODO: Make DirectX headers for MinGW a package and make the DirectX sound
#       driver a component.

def downloadPackage(package, tarballsDir):
	if not isdir(tarballsDir):
		makedirs(tarballsDir)
	if isfile(joinpath(tarballsDir, package.getTarballName())):
		print '%s version %s - already downloaded' % (
			package.niceName, package.version
			)
	else:
		downloadURL(package.getURL(), tarballsDir)

def extractPackage(package, tarballsDir, sourcesDir, patchesDir):
	if not isdir(sourcesDir):
		makedirs(sourcesDir)
	sourceDirName = package.getSourceDirName()
	packageSrcDir = joinpath(sourcesDir, sourceDirName)
	if isdir(packageSrcDir):
		rmtree(packageSrcDir)
	extract(
		joinpath(tarballsDir, package.getTarballName()),
		sourcesDir,
		TopLevelDirRenamer(sourceDirName)
		)
	diffPath = joinpath(patchesDir, sourceDirName + '.diff')
	if isfile(diffPath):
		for diff in Diff.load(diffPath):
			patch(diff, sourcesDir)
			print 'Patched:', diff.getPath()

def main(platform, tarballsDir, sourcesDir, patchesDir):
	configuration = getConfiguration('3RD_STA')

	# Compute the set of all directly and indirectly required libraries.
	transLibs = set()
	for makeName in requiredLibrariesFor(configuration.iterDesiredComponents()):
		library = librariesByName[makeName]
		transLibs.add(makeName)
		for depMakeName in library.dependsOn:
			transLibs.add(depMakeName)
	# Filter out system libraries.
	thirdPartyLibs = set(
		makeName
		for makeName in transLibs
		if not librariesByName[makeName].isSystemLibrary(platform)
		)

	for makeName in sorted(thirdPartyLibs):
		package = getPackage(makeName)
		downloadPackage(package, tarballsDir)
		extractPackage(package, tarballsDir, sourcesDir, patchesDir)

if __name__ == '__main__':
	if len(sys.argv) == 2:
		main(
			sys.argv[1],
			'derived/3rdparty/download',
			'derived/3rdparty/src',
			'build/3rdparty'
			)
	else:
		print >> sys.stderr, (
			'Usage: python 3rdparty_download.py TARGET_OS'
			)
		sys.exit(2)
