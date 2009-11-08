# $Id$

from packages import getPackage, iterDownloadablePackages

import sys

makeNames = sorted(
	package.getMakeName() for package in iterDownloadablePackages()
	)

def genDownloadURLs():
	yield '# Download locations for package sources.'
	for makeName in makeNames:
		package = getPackage(makeName)
		yield 'DOWNLOAD_%s:=%s' % (makeName, package.downloadURL)

def genVersions():
	yield '# The package versions that we use.'
	yield '# You can select different versions by editing "build/packages.py".'
	for makeName in makeNames:
		package = getPackage(makeName)
		yield 'PACKAGE_%s:=%s' % (makeName, package.getSourceDirName())

def genTarballs():
	yield '# Source tar file names.'
	for makeName in makeNames:
		package = getPackage(makeName)
		yield 'TARBALL_%s:=%s' % (makeName, package.getTarballName())

def printPackagesMake():
	for line in genDownloadURLs():
		print line
	print
	for line in genVersions():
		print line
	print
	for line in genTarballs():
		print line

if __name__ == '__main__':
	if len(sys.argv) == 1:
		printPackagesMake()
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_packages2make.py'
		sys.exit(2)
