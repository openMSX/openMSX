# $Id$
# Perform a sanity check on the compiler.

from compilers import tryCompile

import sys

def checkCompiler(log, compileEnv, compileCommand, compileFlags, outDir):
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
	return tryCompile(
		log, compileEnv, compileCommand, compileFlags,
		outDir + '/hello.cc', hello()
		)

def main(compileCommandStr, compileFlagsStr, outDir, logPath, makePath):
	log = open(logPath, 'a')
	try:
		compileCommand = compileCommandStr.split()
		compileFlags = compileFlagsStr.split()
		compileEnv = {}
		while compileCommand:
			if '=' in compileCommand[0]:
				name, value = compileCommand[0].split('=', 1)
				del compileCommand[0]
				compileEnv[name] = value
			else:
				compileFlags = compileCommand[1 : ] + compileFlags
				ok = checkCompiler(
					log, compileEnv, compileCommand[0], compileFlags, outDir
					)
				print >> log, 'Compiler %s: %s' % (
					'works' if ok else 'broken',
					' '.join(
						[ compileCommand[0] ] + compileFlags +
						([ '(%s)' % ' '.join(
							'%s=%s' % item
							for item in sorted(compileEnv.iteritems())
							) ] if compileEnv else [])
						),
					)
				break
		else:
			ok = False
			print >> log, 'No compiler specified in "%s"' % compileCommandStr

	finally:
		log.close()

	make = open(makePath, 'a')
	try:
		print >> make, 'COMPILER:=%s' % str(ok).lower()
	finally:
		make.close()

if __name__ == '__main__':
	if len(sys.argv) == 6:
		main(*sys.argv[1 : ])
	else:
		print >> sys.stderr, (
			'Usage: python probe_run_compiler.py '
			'COMPILE COMPILE_FLAGS OUTDIR LOG OUTMAKE'
			)
		sys.exit(2)
