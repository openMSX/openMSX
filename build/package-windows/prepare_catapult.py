# Converts the extracted wxCatapult CI artifact (x64, Release) into the
# directory layout that packagemsi.py expects as its catapultPath.
#
# Usage: py prepare_catapult.py <extracted artifact dir> <output dir>

from os import makedirs
from os.path import dirname, join as joinpath
from shutil import copy2, copytree, ignore_patterns
import sys

buildPath = 'derived/x64-VC-Unicode Release'

def prepareCatapult(artifactPath, outPath):
	def src(path):
		return joinpath(artifactPath, 'Catapult', path)
	def dst(path):
		return joinpath(outPath, path)
	def copyFile(source, dest):
		makedirs(dirname(dest), exist_ok = True)
		copy2(source, dest)

	copyFile(src('bin/catapult.exe'), dst(buildPath + '/install/catapult.exe'))
	# README is packaged separately; keeping it in doc too would install it
	# twice into Catapult\doc.
	copytree(src('doc'), dst('doc'), ignore = ignore_patterns('README'))
	copyFile(src('doc/README'), dst('README'))
	copytree(src('resources/bitmaps'), dst('resources/bitmaps'))
	copytree(src('resources/dialogs'), dst(buildPath + '/install/dialogs'))
	copyFile(
		src('resources/icons/catapult.xpm'), dst(buildPath + '/src/catapult.xpm')
		)

if __name__ == '__main__':
	if len(sys.argv) == 3:
		prepareCatapult(*sys.argv[1 : ])
	else:
		print(
			'Usage: py prepare_catapult.py artifactPath outPath',
			file=sys.stderr
			)
		sys.exit(2)
