from packages import getPackage, iterDownloadablePackages

import sys

def printPackagesMake():
	patchesDir = 'build/3rdparty'
	sourceDir = 'derived/3rdparty/src'
	tarballsDir = 'derived/3rdparty/download'
	print('SOURCE_DIR:=%s' % sourceDir)
	print()

	print('# Information about packages.')
	print('# Generated from the data in "build/packages.py".')
	print()
	tarballs = []
	for package in iterDownloadablePackages():
		makeName = package.getMakeName()
		tarball = tarballsDir + '/' + package.getTarballName()
		tarballs.append(tarball)
		print('# %s' % package.niceName)
		print('PACKAGE_%s:=%s' % (makeName, package.getSourceDirName()))
		print('TARBALL_%s:=%s' % (makeName, tarball))
		print('# Download:')
		print('%s:' % tarball)
		print('\tmkdir -p %s' % tarballsDir)
		print('\t$(PYTHON) build/download.py %s/%s %s' % (
			package.downloadURL, package.getTarballName(), tarballsDir
			))
		packageSourceDirName = package.getSourceDirName()
		packageSourceDir = sourceDir + '/' + packageSourceDirName
		patchFile = '%s/%s.diff' % (patchesDir, packageSourceDirName)
		print('# Verify:')
		verifyMarker = '%s.verified' % tarball
		print('%s: %s' % (verifyMarker, tarball))
		print('\t$(PYTHON) build/checksum.py %s %d %s' % (
			tarball,
			package.fileLength,
			' '.join('%s=%s' % item for item in package.checksums.items())
			))
		print('\ttouch %s' % verifyMarker)
		print('# Extract:')
		extractMarker = '%s/.extracted' % packageSourceDir
		print('%s: %s $(wildcard %s)' % (extractMarker, verifyMarker, patchFile))
		print('\trm -rf %s' % packageSourceDir)
		print('\tmkdir -p %s' % sourceDir)
		print('\t$(PYTHON) build/extract.py %s %s %s' % (
			tarball, sourceDir, packageSourceDirName
			))
		print('\ttest ! -e %s || $(PYTHON) build/patch.py %s %s' % (
			patchFile, patchFile, sourceDir
			))
		print('\ttouch %s' % extractMarker)
		print()

	print('# Convenience target to download all source packages.')
	print('.PHONY: download')
	print('download: %s' % ' '.join(tarballs))

if __name__ == '__main__':
	if len(sys.argv) == 1:
		printPackagesMake()
	else:
		print('Usage: python3 3rdparty_packages2make.py', file=sys.stderr)
		sys.exit(2)
