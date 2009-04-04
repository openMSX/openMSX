# $Id$
# Check the existence of a given library.

from compilers import CompileCommand, LinkCommand, tryLink

import sys

def checkLib(log, compileCommand, linkCommand, outDir, makeName):
	'''Checks whether the given library can be linked against.
	Returns True iff the library is available.
	'''
	return tryLink(
		log, compileCommand, linkCommand, outDir + '/' + makeName + '.cc'
		)

def main(
	compileCommandStr, compileFlagsStr, outDir, logPath, makePath,
	makeName
	):
	# TODO: We provide the same flags to the compile and link step, because
	#       that seems to work in practice. In the future, use compiile
	#       flags and link flags only for their respective steps.
	compileCommand = CompileCommand.fromLine(
		compileCommandStr, compileFlagsStr
		)
	linkCommand = LinkCommand.fromLine(
		compileCommandStr, compileFlagsStr
		)
	log = open(logPath, 'a')
	try:
		ok = checkLib(
			log, compileCommand, linkCommand, outDir, makeName
			)
		print >> log, '%s library: %s' % (
			'Found' if ok else 'Missing',
			makeName
			)
	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'HAVE_%s_LIB:=%s' % (makeName,'yes' if ok else '')
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 7:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_library.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE MAKENAME'
			)
		sys.exit(2)
