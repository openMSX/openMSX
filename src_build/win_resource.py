# Generates Windows resource header.

from outpututils import rewriteIfChanged
from version import (
	extractRevisionNumber, getDetailedVersion, packageVersionNumber
)

import sys

def iterResourceHeader():
	versionComponents = packageVersionNumber.split('.')
	versionComponents += ['0'] * (3 - len(versionComponents))
	versionComponents.append(str(extractRevisionNumber()))
	assert len(versionComponents) == 4, versionComponents

	yield '#define OPENMSX_VERSION_INT %s' % ', '.join(versionComponents)
	yield '#define OPENMSX_VERSION_STR "%s\\0"' % getDetailedVersion()

if __name__ == '__main__':
	if len(sys.argv) == 2:
		rewriteIfChanged(sys.argv[1], iterResourceHeader())
	else:
		print('Usage: python3 win-resource.py RESOURCE_HEADER', file=sys.stderr)
		sys.exit(2)
