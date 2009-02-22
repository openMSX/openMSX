# $Id$

import sys

from fileutils import install, scanTree

def main(srcDir, selection, destDir):
	if selection == [ '.' ]:
		selection = None
	try:
		install(srcDir, destDir, scanTree(srcDir, selection))
	except ValueError, ex:
		print >> sys.stderr, 'Invalid argument:', ex
		sys.exit(2)
	except IOError, ex:
		print >> sys.stderr, 'Installation failed:', ex
		sys.exit(1)

if len(sys.argv) >= 4:
	main(sys.argv[1], sys.argv[2 : -1], sys.argv[-1])
else:
	print >> sys.stderr, \
		'Usage: python install-recursive.py ' \
		'src-base (src-file|src-dir)+ dst-dir'
	sys.exit(2)
