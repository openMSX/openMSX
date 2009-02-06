// $Id$

#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace openmsx {

HostCPU::HostCPU()
{
	// Assume no CPU feature support
	mmxFlag  = false;
	sseFlag  = false;
	sse2Flag = false;

	#ifdef __x86_64
	// X86_64 machines always have mmx, sse, sse2
	mmxFlag  = true;
	sseFlag  = true;
	sse2Flag = true;
	#elif defined ASM_X86_32
	#ifdef _MSC_VER
	unsigned hasCPUID;
	__asm {
		// Load EFLAGS into EAX
		pushf
		pop		eax
		// Save current value.
		mov		ecx,eax
		// Toggle bit 21.
		xor		eax,200000h
		// Load EAX into EFLAGS.
		push	eax
		popf
		// Load EFLAGS into EAX.
		pushf
		pop		eax
		// Did bit 21 change?
		xor		eax,ecx
		and		eax,200000h
		mov		hasCPUID,eax
	}
	if (hasCPUID) {
		int cpuInfo[4];
		__cpuid(cpuInfo, 0);
		if (cpuInfo[0] >= 1) {
			__cpuid(cpuInfo, 1);
			setFeatures(cpuInfo[3]);
		}
	}
	#else
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
				setFeatures(features);
			}
		}
	#endif
	#endif

	PRT_DEBUG("MMX:  " << mmxFlag);
	PRT_DEBUG("SSE:  " << sseFlag);
	PRT_DEBUG("SSE2: " << sse2Flag);

	if (hasSSE2()) { assert(hasMMX() && hasSSE()); }
	if (hasSSE())  { assert(hasMMX()); }
}

void HostCPU::setFeatures(unsigned features) {

	const unsigned CPUID_FEATURE_MMX	= 0x0800000;
	const unsigned CPUID_FEATURE_SSE	= 0x2000000;
	const unsigned CPUID_FEATURE_SSE2	= 0x4000000;

	mmxFlag  = (features & CPUID_FEATURE_MMX)	!= 0;
	sseFlag  = (features & CPUID_FEATURE_SSE)	!= 0;
	sse2Flag = (features & CPUID_FEATURE_SSE2)	!= 0;
}

} // namespace openmsx
