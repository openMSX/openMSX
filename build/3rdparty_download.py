# $Id$

from components import requiredLibrariesFor
from configurations import getConfiguration
from download import downloadURL
from libraries import librariesByName
from packages import getPackage

from os.path import isdir, isfile, join as joinpath
import sys

# TODO: Make DirectX headers for MinGW a package and make the DirectX sound
#       driver a component.

def downloadPackages(tarballsDir, makeNames):
	for makeName in sorted(makeNames):
		package = getPackage(makeName)
		if isfile(joinpath(tarballsDir, package.getTarballName())):
			print '%s version %s - already downloaded' % (
				package.niceName, package.version
				)
		else:
			downloadURL(package.getURL(), tarballsDir)

def main(platform, tarballsDir):
	if not isdir(tarballsDir):
		print >> sys.stderr, \
			'Output directory "%s" does not exist' % tarballsDir
		sys.exit(2)

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

	downloadPackages(tarballsDir, thirdPartyLibs)

if __name__ == '__main__':
	if len(sys.argv) == 3:
		main(*sys.argv[1 : ])
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_download.py TARGET_OS TARBALLS_DIR'
		sys.exit(2)
