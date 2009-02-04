// $Id$

#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

HostCPU::HostCPU()
{
	mmxFlag  = false;
	sseFlag  = false;
	sse2Flag = false;
	#ifdef ASM_X86_32
		// Note: On Mac OS X, EBX is in use by the OS,
		//       so we have to restore it.
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
				mmxFlag  = features & 0x0800000;
				sseFlag  = features & 0x2000000;
				sse2Flag = features & 0x4000000;
			}
		}
	#endif
	#ifdef __x86_64
		// X86_64 machines always have mmx, sse, sse2
		mmxFlag  = true;
		sseFlag  = true;
		sse2Flag = true;
	#endif

	PRT_DEBUG("MMX:  " << mmxFlag);
	PRT_DEBUG("SSE:  " << sseFlag);
	PRT_DEBUG("SSE2: " << sse2Flag);

	if (hasSSE2()) { assert(hasMMX() && hasSSE()); }
	if (hasSSE())  { assert(hasMMX()); }
}

} // namespace openmsx
