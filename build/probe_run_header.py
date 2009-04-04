# $Id$
# Check the existence of a given header.

from compilers import CompileCommand, tryCompile

import sys

def checkFunc(log, compileCommand, outDir, makeName, headers):
	'''Checks whether the given headers can be included.
	Returns True iff the headers are available.
	'''
	def includeHeaders():
		# Try to include the headers.
		for header in headers:
			yield '#include %s' % header
	return tryCompile(
		log, compileCommand, outDir + '/' + makeName + '.cc', includeHeaders()
		)

def main(
	compileCommandStr, compileFlagsStr, outDir, logPath, makePath,
	makeName, headerStr
	):
	compileCommand = CompileCommand.fromLine(compileCommandStr, compileFlagsStr)
	log = open(logPath, 'a')
	try:
		ok = checkFunc(
			log, compileCommand, outDir, makeName, headerStr.split()
			)
		print >> log, '%s header: %s' % (
			'Found' if ok else 'Missing',
			makeName
			)
	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'HAVE_%s_H:=%s' % (makeName,'true' if ok else '')
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 8:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_header.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE MAKENAME HEADERS'
			)
		sys.exit(2)
