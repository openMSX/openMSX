# Detect the native CPU and OS.
# Actually we rely on the Python "platform" module and map its output to names
# that the openMSX build understands.

from executils import captureStdout

from platform import architecture, machine, system
from subprocess import PIPE, STDOUT, Popen
import sys

def detectCPU():
	'''Detects the CPU family (not the CPU model) of the machine were are
	running on.
	Raises ValueError if no known CPU is detected.
	'''
	cpu = machine().lower()
	dashIndex = cpu.find('-')
	if dashIndex != -1:
		# Hurd returns "cputype-cpusubtype" instead of just "cputype".
		cpu = cpu[ : dashIndex]
	if cpu in ('x86_64', 'amd64'):
		return 'x86_64'
	elif cpu in ('x86', 'i386', 'i486', 'i586', 'i686'):
		return 'x86'
	elif cpu == 'ppc64le':
		return 'ppc64le'
	elif cpu.startswith('ppc') or cpu.endswith('ppc') or cpu.startswith('power'):
		return 'ppc64' if cpu.endswith('64') else 'ppc'
	elif cpu == 'arm64':
		# Darwin uses "arm64", unlike Linux.
		return 'aarch64'
	elif cpu.startswith('arm'):
		return 'arm'
	elif cpu == 'aarch64':
		return 'aarch64'
	elif cpu == 'aarch64_be':
		return 'aarch64_be'
	elif cpu.startswith('mips') or cpu == 'sgi':
		return 'mipsel' if cpu.endswith('el') else 'mips'
	elif cpu == 'm68k':
		return 'm68k'
	elif cpu == 'ia64':
		return 'ia64'
	elif cpu.startswith('alpha'):
		return 'alpha'
	elif cpu.startswith('hppa') or cpu.startswith('parisc'):
		return 'hppa'
	elif cpu.startswith('s390'):
		return 's390'
	elif cpu.startswith('sparc') or cpu.startswith('sun4u'):
		return 'sparc'
	elif cpu.startswith('sh'):
		return 'sheb' if cpu.endswith('eb') else 'sh'
	elif cpu == 'avr32':
		return 'avr32'
	elif cpu == 'riscv64':
		return 'riscv64'
	elif cpu == 'loongarch64':
		return 'loongarch64'
	elif cpu == '':
		# Python couldn't figure it out.
		os = system().lower()
		if os == 'windows':
			# Relatively safe bet.
			return 'x86'
		raise ValueError('Unable to detect CPU')
	else:
		raise ValueError('Unsupported or unrecognised CPU "%s"' % cpu)

def detectOS():
	'''Detects the operating system of the machine were are running on.
	Raises ValueError if no known OS is detected.
	'''
	os = system().lower()
	if os in (
		'linux', 'darwin', 'freebsd', 'netbsd', 'openbsd', 'dragonfly', 'gnu'
		):
		return os
	elif os.startswith('gnu/'):
		# GNU userland on non-Hurd kernel, for example Debian GNU/kFreeBSD.
		# For openMSX the kernel is not really relevant, so treat it like
		# a generic GNU system.
		return 'gnu'
	elif os.startswith('mingw') or os == 'windows':
		return 'mingw-w64'
	elif os == 'sunos':
		return 'solaris'
	elif os == '':
		# Python couldn't figure it out.
		raise ValueError('Unable to detect OS')
	else:
		raise ValueError('Unsupported or unrecognised OS "%s"' % os)

def getCompilerMachine():
	# Note: Recent GCC and Clang versions support this option.
	machine = captureStdout(sys.stderr, 'cc -dumpmachine')
	if machine is not None:
		machineParts = machine.split('-')
		if len(machineParts) >= 3:
			return machineParts[0], machineParts[2]
	return None, None

if __name__ == '__main__':
	try:
		hostCPU = detectCPU()
		if hostCPU == 'mips':
			# Little endian MIPS is reported as just "mips" by Linux Python.
			compilerCPU, compilerOS = getCompilerMachine()
			if compilerCPU == 'mips':
				pass
			elif compilerCPU == 'mipsel':
				hostCPU = compilerCPU
			else:
				print(
					'Warning: Unabling to determine endianess; '
					'compiling for big endian',
					file=sys.stderr
					)

		hostOS = detectOS()
		if hostOS == 'mingw32' and hostCPU == 'x86_64':
			# It is possible to run MinGW on 64-bit Windows, but producing
			# 64-bit code is not supported yet.
			hostCPU = 'x86'
		elif hostOS == 'darwin' and hostCPU == 'x86':
			# If Python is 64-bit, both the CPU and OS support it, so we can
			# compile openMSX for x86-64. Compiling in 32-bit mode might seem
			# safer, but will fail if using MacPorts on a 64-bit capable system.
			if architecture()[0] == '64bit':
				hostCPU = 'x86_64'

		print(hostCPU, hostOS)
	except ValueError as ex:
		print(ex, file=sys.stderr)
		sys.exit(1)
