# $Id$
# Detect the native CPU and OS.
# Actually we rely on the Python "platform" module and map its output to names
# that the openMSX build understands.

from platform import machine, system
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
	elif cpu.startswith('ppc') or cpu.startswith('power'):
		return 'ppc64' if cpu.endswith('64') else 'ppc'
	elif cpu.startswith('arm'):
		return 'arm'
	elif cpu.startswith('mips'):
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
	if os in ('linux', 'darwin', 'freebsd', 'netbsd', 'openbsd', 'gnu'):
		return os
	elif os.startswith('gnu/'):
		# GNU userland on non-Hurd kernel, for example Debian GNU/kFreeBSD.
		# For openMSX the kernel is not really relevant, so treat it like
		# a generic GNU system.
		return 'gnu'
	elif os.startswith('mingw') or os == 'windows':
		return 'mingw32'
	elif os == 'sunos':
		return 'solaris'
	elif os == '':
		# Python couldn't figure it out.
		raise ValueError('Unable to detect OS')
	else:
		raise ValueError('Unsupported or unrecognised OS "%s"' % os)

if __name__ == '__main__':
	try:
		hostCPU = detectCPU()
		hostOS = detectOS()
		if hostOS == 'mingw32' and hostCPU == 'x86_64':
			# It is possible to run MinGW on 64-bit Windows, but producing
			# 64-bit code is not supported yet.
			hostCPU = 'x86'
		print '%s-%s' % (hostCPU, hostOS)
	except ValueError, ex:
		print >> sys.stderr, ex
		sys.exit(1)
