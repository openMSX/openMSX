# Prints the Make names of those libraries that are present in the base OS.

from libraries import librariesByName

def main(platform):
	systemLibs = set(
		name
		for name, library in librariesByName.iteritems()
		if library.isSystemLibrary(platform)
		)
	print ' '.join(sorted(systemLibs))

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 2:
		main(*sys.argv[1 : ])
	else:
		print >> sys.stderr, (
			'Usage: python list_system_libs.py TARGET_OS'
			)
		sys.exit(2)
