# Prints the Make names of those libraries that are present in the base OS.

from libraries import librariesByName

def main(platform):
	systemLibs = set(
		name
		for name, library in librariesByName.items()
		if library.isSystemLibrary(platform)
		)
	print(' '.join(sorted(systemLibs)))

if __name__ == '__main__':
	import sys
	if len(sys.argv) == 2:
		main(*sys.argv[1 : ])
	else:
		print('Usage: python3 list_system_libs.py TARGET_OS', file=sys.stderr)
		sys.exit(2)
