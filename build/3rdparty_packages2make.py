# $Id$

from packages import getPackage, iterDownloadablePackages

import sys

def printPackagesMake():
	patchesDir = 'build/3rdparty'
	sourceDir = 'derived/3rdparty/src'
	tarballsDir = 'derived/3rdparty/download'
	print 'SOURCE_DIR:=%s' % sourceDir
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
		print '# Download:'
		print '%s:' % tarball
		print '\tmkdir -p %s' % tarballsDir
		print '\t$(PYTHON) build/download.py %s/%s %s' % (
			package.downloadURL, package.getTarballName(), tarballsDir
			)
		packageSourceDirName = package.getSourceDirName()
		packageSourceDir = sourceDir + '/' + packageSourceDirName
		patchFile = '%s/%s.diff' % (patchesDir, packageSourceDirName)
		print '# Extract:'
		print '%s: %s' % (packageSourceDir, tarball)
		print '\trm -rf %s' % packageSourceDir
		print '\tmkdir -p %s' % sourceDir
		print '\t$(PYTHON) build/extract.py %s %s %s' % (
			tarball, sourceDir, packageSourceDirName
			)
		print '\ttest ! -e %s || $(PYTHON) build/patch.py %s %s' % (
			patchFile, patchFile, sourceDir
			)
		print '\ttouch %s' % sourceDir
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
