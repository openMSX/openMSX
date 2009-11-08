# $Id$

from packages import getPackage, iterDownloadablePackages

import sys

def printPackagesMake():
	print '# Information about packages.'
	print '# Generated from the data in "build/packages.py".'
	print
	for package in iterDownloadablePackages():
		makeName = package.getMakeName()
		print '# %s' % package.niceName
		print 'PACKAGE_%s:=%s' % (makeName, package.getSourceDirName())
		print 'TARBALL_%s:=%s' % (makeName, package.getTarballName())
		print 'DOWNLOAD_%s:=%s' % (makeName, package.downloadURL)
		print

if __name__ == '__main__':
	if len(sys.argv) == 1:
		printPackagesMake()
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_packages2make.py'
		sys.exit(2)
