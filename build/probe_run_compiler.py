# $Id$
# Perform a sanity check on the compiler.

from compilers import CompileCommand, tryCompile

import sys

def checkCompiler(log, compileCommand, outDir):
	'''Checks whether compiler can compile anything at all.
	Returns True iff the compiler works.
	'''
	def hello():
		# The most famous program.
		yield '#include <iostream>'
		yield 'int main(int argc, char** argv) {'
		yield '  std::cout << "Hello World!" << std::endl;'
		yield '  return 0;'
		yield '}'
	return tryCompile(log, compileCommand, outDir + '/hello.cc', hello())

def main(compileCommandStr, compileFlagsStr, outDir, logPath, makePath):
	compileCommand = CompileCommand.fromLine(compileCommandStr, compileFlagsStr)
	log = open(logPath, 'a')
	try:
		ok = checkCompiler(log, compileCommand, outDir)
		print >> log, 'Compiler %s: %s' % (
			'works' if ok else 'broken',
			compileCommand
			)
	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'COMPILER:=%s' % str(ok).lower()
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 6:
		try:
			main(*sys.argv[1 : ])
		except ValueError, ex:
			print >> sys.stderr, ex
			sys.exit(2)
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_compiler.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE'
			)
		sys.exit(2)
