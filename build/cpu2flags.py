from cpu import getCPU

import sys

def getCPUFlags(cpuName):
	cpu = getCPU(cpuName)
	return ' '.join(cpu.gccFlags)

if __name__ == '__main__':
	if len(sys.argv) == 2:
		cpuName = sys.argv[1]
		try:
			print getCPUFlags(cpuName)
		except KeyError:
			print >> sys.stderr, 'Unknown CPU "%s"' % cpuName
	else:
		print >> sys.stderr, 'Usage: python cpu2flags.py OPENMSX_TARGET_CPU'
		sys.exit(2)
