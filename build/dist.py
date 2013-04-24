# Creates a source distribution package.

from fileutils import installDirs, installTree, scanTree
from version import getVersionedPackageName

from os.path import isdir, islink, join as joinpath
from shutil import rmtree
import sys
import tarfile

def main(paths):
	versionedPackageName = getVersionedPackageName()
	distBase = 'derived/dist/'
	distPath = distBase + versionedPackageName + '/'

	if isdir(distPath):
		print 'Removing old distribution files...'
		if islink(distPath):
			# Note: Python 2.6 does this check and it seems like a sensible
			#       precaution; since we support Python 2.5 as well we
			#       explictly perform the check before calling rmtree().
			raise OSError(
				'Refusing to recursively delete symlinked directory "%s"'
				% distPath
				)
		rmtree(distPath)

	print 'Gathering files for distribution...'
	def gather():
		for path in paths:
			if isdir(path):
				for relpath in scanTree(path):
					yield joinpath(path, relpath)
			else:
				yield path
	installDirs(distPath)
	installTree('.', distPath, gather())

	print 'Creating tarball...'
	# TODO: Try to generate the tarball directly from the provided file lists,
	#       without creating a copy in the "dist" directory first.
	tar = tarfile.open(distBase + versionedPackageName + '.tar.gz', 'w:gz')
	try:
		tar.add(distPath, versionedPackageName)
	finally:
		tar.close()

if __name__ == '__main__':
	if len(sys.argv) > 1:
		main(sys.argv[1 : ])
	else:
		print >> sys.stderr, 'Usage: python dist.py paths'
		sys.exit(2)
