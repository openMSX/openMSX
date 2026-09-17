from checksum import verifyFile
from components import requiredLibrariesFor
from configurations import getConfiguration
from download import downloadURL
from extract import TopLevelDirRenamer, extract
from libraries import allDependencies, librariesByName
from packages import getPackage
from patch import Diff, patch

from os import makedirs, remove
from os.path import isdir, isfile, join as joinpath
from shutil import rmtree
from time import sleep
import sys

# TODO: Make DirectX headers for MinGW a package and make the DirectX sound
#       driver a component.

NUM_DOWNLOAD_ATTEMPTS = 3
DOWNLOAD_RETRY_DELAY = 5 # seconds

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
	'''Returns None if the tarball is intact, or a description of what is wrong
	with it otherwise.
	'''
	filePath = joinpath(tarballsDir, package.getTarballName())
	try:
		verifyFile(filePath, package.fileLength, package.checksums)
	except OSError as ex:
		return str(ex)
	else:
		return None

def fetchPackage(package, tarballsDir):
	'''Downloads the tarball and checks it, retrying a couple of times. A
	download can simply fail, but a server can also hand out an error page
	instead of the file, which only shows up when verifying it.
	'''
	filePath = joinpath(tarballsDir, package.getTarballName())
	for attempt in range(NUM_DOWNLOAD_ATTEMPTS):
		try:
			downloadPackage(package, tarballsDir)
		except OSError as ex:
			problem = str(ex)
		else:
			problem = verifyPackage(package, tarballsDir)
			if problem is None:
				return
			problem = 'corrupt: %s' % problem
		# Throw it away, so that the next attempt downloads it again.
		if isfile(filePath):
			remove(filePath)
		# Keep the order of the messages in the output intact.
		sys.stdout.flush()
		print('%s %s' % (package.getTarballName(), problem), file=sys.stderr)
		if attempt + 1 != NUM_DOWNLOAD_ATTEMPTS:
			print('Retrying in %d seconds (attempt %d of %d)' % (
				DOWNLOAD_RETRY_DELAY, attempt + 2, NUM_DOWNLOAD_ATTEMPTS
				), file=sys.stderr)
			sys.stderr.flush()
			sleep(DOWNLOAD_RETRY_DELAY)
	print('Giving up on %s after %d attempts' % (
		package.getTarballName(), NUM_DOWNLOAD_ATTEMPTS
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
		fetchPackage(package, tarballsDir)
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
