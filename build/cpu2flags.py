from cpu import getCPU

import sys

def getCPUFlags(cpuName):
	cpu = getCPU(cpuName)
	return ' '.join(cpu.gccFlags)

if __name__ == '__main__':
	if len(sys.argv) == 2:
		cpuName = sys.argv[1]
		try:
			print(getCPUFlags(cpuName))
		except KeyError:
			print('Unknown CPU "%s"' % cpuName, file=sys.stderr)
	else:
		print('Usage: python3 cpu2flags.py OPENMSX_TARGET_CPU', file=sys.stderr)
		sys.exit(2)
