# Generates Windows resource header.

from outpututils import rewriteIfChanged
from version import extractRevisionNumber, packageVersion

import sys

def iterResourceHeader():
	if '-' in packageVersion:
		versionNumber = packageVersion[ : packageVersion.index('-')]
	else:
		versionNumber = packageVersion
	revision = str(extractRevisionNumber())
	versionComponents = versionNumber.split('.') + [ revision ]
	assert len(versionComponents) == 4, versionComponents

	yield '#define OPENMSX_VERSION_INT %s' % ', '.join(versionComponents)
	yield '#define OPENMSX_VERSION_STR "%s\\0"' % packageVersion

if __name__ == '__main__':
	if len(sys.argv) == 2:
		rewriteIfChanged(sys.argv[1], iterResourceHeader())
	else:
		print >> sys.stderr, \
			'Usage: python win-resource.py RESOURCE_HEADER'
		sys.exit(2)
