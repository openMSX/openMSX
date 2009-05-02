# $Id$

from download import downloadURL
from packages import (
	DirectX, FreeType, GLEW, LibPNG, LibXML2, SDL, SDL_image, SDL_ttf, TCL, ZLib
	)

from os.path import isdir, isfile, join as joinpath
import sys

#TODO:
#	Specify downloads for:
#	- MINGW (DX7)
#	- VC++
#	- OTHER
#
# required components -> required packages -> minus system packages
#   -> 3rdparty packages

def downloadPackages(packages):
	for package in packages:
		if isfile(joinpath(tarballsDir, package.getTarballName())):
			print '%s version %s - already downloaded' % (
				package.niceName, package.version
				)
		else:
			downloadURL(package.getURL(), tarballsDir)

if __name__ == '__main__':
	if len(sys.argv) == 3:
		platform = sys.argv[1] # One of { Win32, x64, mingw }
		tarballsDir = sys.argv[2]

		#Make Package selection
		if platform == 'Win32':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, FreeType,
				LibXML2
				)
		elif platform == 'x64':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, FreeType,
				LibXML2
				)
		elif platform == 'mingw':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, FreeType,
				LibXML2, DirectX
				)
			#add platform etc
		else:
			print >> sys.stderr, 'Unknown platform "%s"' % platform
			sys.exit(2)

		if not isdir(tarballsDir):
			print >> sys.stderr, \
				'Output directory "%s" does not exist' % tarballsDir
			sys.exit(2)

		downloadPackages(download_packages)
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_download.py platform TARBALLS_DIR'
		sys.exit(2)
