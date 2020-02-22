from checksum import verifyFile
from components import requiredLibrariesFor
from configurations import getConfiguration
from download import downloadURL
from extract import TopLevelDirRenamer, extract
from libraries import allDependencies, librariesByName
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
	filePath = joinpath(tarballsDir, package.getTarballName())
	if isfile(filePath):
		print('%s version %s - already downloaded' % (
			package.niceName, package.version
			))
	else:
		downloadURL(package.getURL(), tarballsDir)

def verifyPackage(package, tarballsDir):
	filePath = joinpath(tarballsDir, package.getTarballName())
	try:
		verifyFile(filePath, package.fileLength, package.checksums)
	except OSError as ex:
		print('%s corrupt: %s' % (
			package.getTarballName(), ex
			), file=sys.stderr)
		sys.exit(1)

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
			print('Patched:', diff.getPath())


def fetchPackageSource(makeName, tarballsDir, sourcesDir, patchesDir):
		package = getPackage(makeName)
		downloadPackage(package, tarballsDir)
		verifyPackage(package, tarballsDir)
		extractPackage(package, tarballsDir, sourcesDir, patchesDir)

def main(platform, tarballsDir, sourcesDir, patchesDir):
	configuration = getConfiguration('3RD_STA')
	components = configuration.iterDesiredComponents()

	# Compute the set of all directly and indirectly required libraries,
	# then filter out system libraries.
	thirdPartyLibs = set(
		makeName
		for makeName in allDependencies(requiredLibrariesFor(components))
		if not librariesByName[makeName].isSystemLibrary(platform)
		)

	if platform == 'windows':
		# Avoid ALSA, since we won't be building it and extracting it will
		# fail on file systems that don't support symlinks.
		# TODO: 3rdparty.mk filters out ALSA on non-Linux platforms;
		#       figure out a way to do that in a single location.
		thirdPartyLibs.discard('ALSA')

	for makeName in sorted(thirdPartyLibs):
		fetchPackageSource(makeName, tarballsDir, sourcesDir, patchesDir)

if __name__ == '__main__':
	if len(sys.argv) == 2:
		main(
			sys.argv[1],
			'derived/3rdparty/download',
			'derived/3rdparty/src',
			'build/3rdparty'
			)
	else:
		print('Usage: python3 thirdparty_download.py TARGET_OS', file=sys.stderr)
		sys.exit(2)
