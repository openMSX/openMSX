# $Id$

from packages import getPackage, iterDownloadablePackages

import sys

def printPackagesMake():
	tarballsDir = 'derived/3rdparty/download'
	print 'TARBALLS_DIR:=%s' % tarballsDir
	print

	print '# Information about packages.'
	print '# Generated from the data in "build/packages.py".'
	print
	tarballs = []
	for package in iterDownloadablePackages():
		makeName = package.getMakeName()
		tarball = tarballsDir + '/' + package.getTarballName()
		tarballs.append(tarball)
		print '# %s' % package.niceName
		print 'PACKAGE_%s:=%s' % (makeName, package.getSourceDirName())
		print 'TARBALL_%s:=%s' % (makeName, tarball)
		print '%s:' % tarball
		print '\tmkdir -p %s' % tarballsDir
		print '\t$(PYTHON) build/download.py %s/%s %s' % (
			package.downloadURL,
			package.getTarballName(),
			tarballsDir
			)
		print

	print '# Convenience target to download all source packages.'
	print '.PHONY: download'
	print 'download: %s' % ' '.join(tarballs)

if __name__ == '__main__':
	if len(sys.argv) == 1:
		printPackagesMake()
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_packages2make.py'
		sys.exit(2)
