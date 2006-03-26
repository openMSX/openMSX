// $Id$

#include "HostCPU.hh"
#include "openmsx.hh"

namespace openmsx {

HostCPU::HostCPU()
{
	mmxFlag = false;
	mmxExtFlag = false;
	#ifdef ASM_X86
		// Note: On Mac OS X, EBX is in use by the OS, so we have to restore it.
		// Is CPUID instruction supported?
		unsigned hasCPUID;
		asm (
			// Load EFLAGS into EAX.
			"pushfl;"
			"popl	%%eax;"
			// Save current value.
			"movl	%%eax,%%ecx;"
			// Toggle bit 21.
			"xorl	$0x200000, %%eax;"
			// Load EAX into EFLAGS.
			"pushl	%%eax;"
			"popfl;"
			// Load EFLAGS into EAX.
			"pushfl;"
			"popl	%%eax;"
			// Did bit 21 change?
			"xor	%%ecx, %%eax;"
			"andl	$0x200000, %%eax;"
			: "=a" (hasCPUID) // 0
			: // no input
			: "ecx"
			);
		if (hasCPUID) {
			// Which CPUID calls are supported?
			unsigned highest;
			asm (
				"pushl	%%ebx;"
				"cpuid;"
				"popl	%%ebx;"
				: "=a" (highest) // 0
				: "0" (0) // 1: function
				: "ecx", "edx"
				);
			if (highest >= 1) {
				// Get features flags.
				unsigned features;
				asm (
					"pushl	%%ebx;"
					"cpuid;"
					"popl	%%ebx;"
					: "=d" (features) // 0
					: "a" (1) // 1: function
					: "ecx"
					);
				mmxFlag = features & 0x800000;
				bool sseFlag = features & 0x2000000;
				mmxExtFlag = mmxFlag && sseFlag;
			}
		}
	#endif

	PRT_DEBUG("MMX:              " << mmxFlag);
	PRT_DEBUG("MMX extensions:   " << mmxExtFlag);
}

} // namespace openmsx
