# $Id$
# Generates Windows resource header.

from outpututils import rewriteIfChanged
from version import extractRevision, packageVersion

import sys

def iterResourceHeader():
	if '-' in packageVersion:
		versionNumber = packageVersion[ : packageVersion.index('-')]
	else:
		versionNumber = packageVersion
	revision = extractRevision()
	# we can't extract a revision in a release dist package, fall back to 0
	if revision == 'unknown':
		revision = '0'
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
