# $Id$
# Prints which 3rd party libraries are desired for the given configuration.

from components import requiredLibrariesFor
from configurations import getConfiguration
from libraries import allDependencies, librariesByName
from packages import iterDownloadablePackages

def main(platform):
	configuration = getConfiguration('3RD_STA')
	components = configuration.iterDesiredComponents()

	# Compute the set of all directly and indirectly required libraries,
	# then filter out system libraries.
	thirdPartyLibs = set(
		makeName
		for makeName in allDependencies(requiredLibrariesFor(components))
		if not librariesByName[makeName].isSystemLibrary(platform)
		)

	print ' '.join(sorted(thirdPartyLibs))

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 2:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python 3rdparty_libraries.py TARGET_OS'
			)
		sys.exit(2)
