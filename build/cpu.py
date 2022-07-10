class CPU(object):
	'''Abstract base class for CPU families.
	'''

	# String that we use to identify this CPU type.
	name = None

	# GCC flags to pass to the compile and link commands.
	gccFlags = ()

class Alpha(CPU):
	'''DEC Alpha.
	'''
	name = 'alpha'

class ARM(CPU):
	'''ARM.
	'''
	name = 'arm'

class ARM64(CPU):
	'''ARM 64-bit, little endian mode.
	'''
	name = 'aarch64'

class ARM64BE(CPU):
	'''ARM 64-bit, big endian mode.
	'''
	name = 'aarch64_be'

class AVR32(CPU):
	'''Atmel AVR32, an embedded RISC CPU.
	'''
	name = 'avr32'

class HPPA(CPU):
	'''HP PA-RISC.
	'''
	name = 'hppa'

class IA64(CPU):
	'''Intel Itanium.
	'''
	name = 'ia64'

class M68k(CPU):
	'''Motorola 680x0.
	'''
	name = 'm68k'

class MIPS(CPU):
	'''Big endian MIPS.
	'''
	name = 'mips'

class MIPSel(MIPS):
	'''Little endian MIPS.
	'''
	name = 'mipsel'

class PPC(CPU):
	'''32-bit Power PC.
	'''
	name = 'ppc'

class PPC64(CPU):
	'''64-bit Power PC.
	'''
	name = 'ppc64'

class PPC64LE(CPU):
	'''64-bit Power PC LE.
	'''
	name = 'ppc64le'

class RISCV64(CPU):
	'''64-bit RISC-V.
	'''
	name = 'riscv64'

class S390(CPU):
	'''IBM S/390.
	'''
	name = 's390'

class SH(CPU):
	'''Little endian Renesas SuperH.
	'''
	name = 'sh'

class SHeb(CPU):
	'''Big endian Renesas SuperH.
	'''
	name = 'sheb'

class Sparc(CPU):
	'''Sun Sparc.
	'''
	name = 'sparc'

class X86(CPU):
	'''32-bit x86: Intel Pentium, AMD Athlon etc.
	'''
	name = 'x86'
	gccFlags = '-m32',

class X86_64(CPU):
	'''64-bit x86. Also known as AMD64 or x64.
	'''
	name = 'x86_64'
	gccFlags = '-m64',

# Build a dictionary of CPUs using introspection.
_cpusByName = {
	obj.name: obj
	for obj in locals().values()
	if isinstance(obj, type)
		and issubclass(obj, CPU)
		and obj is not CPU
	}

def getCPU(name):
	return _cpusByName[name]
