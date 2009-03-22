# $Id$
# Check the existence of a certain function in the standard library.

from compilers import CompileCommand

import sys

def checkFunc(log, compileCommand, outDir, makeName, funcName, headers):
	'''Checks whether the given function is declared by the given headers.
	Returns True iff the function is declared.
	'''
	def takeFuncAddr():
		# Try to include the necessary headers and get the function address.
		for header in headers:
			yield '#include %s' % header
		yield 'void (*f)() = reinterpret_cast<void (*)()>(%s);' % funcName
	return compileCommand.tryCompile(
		log, outDir + '/' + makeName + '.cc', takeFuncAddr()
		)

def main(
	compileCommandStr, compileFlagsStr, outDir, logPath, makePath,
	makeName, funcName, headerStr
	):
	compileCommand = CompileCommand.fromLine(compileCommandStr, compileFlagsStr)
	log = open(logPath, 'a')
	try:
		ok = checkFunc(
			log, compileCommand, outDir, makeName, funcName, headerStr.split()
			)
		print >> log, '%s function: %s' % (
			'Found' if ok else 'Missing',
			makeName
			)
	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'HAVE_%s:=%s' % (makeName,'true' if ok else '')
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 9:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_function.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE MAKENAME FUNCNAME HEADERS'
			)
		sys.exit(2)
