# $Id: thirdparty_download.py 11350 2010-03-23 01:18:06Z mthuurne $

from thirdparty_download import fetchPackageSource
import sys

def main(tarballsDir, sourcesDir, patchesDir):
	fetchPackageSource('TCL', tarballsDir, sourcesDir, patchesDir)

if __name__ == '__main__':
	if len(sys.argv) == 1:
		main(
			'derived/3rdparty/download',
			'derived/3rdparty/src',
			'build/3rdparty'
			)
	else:
		print >> sys.stderr, (
			'Usage: python android_download.py'
			)
		sys.exit(2)
