// $Id$

#include "HostCPU.hh"
#include <cassert>
#include <cstdio>


namespace openmsx {

HostCPU::HostCPU()
{
	fprintf(stderr, "probing host CPU...\n");

	mmxFlag = false;
	mmxExtFlag = false;
	if (ASM_X86) {
		// Is CPUID instruction supported?
		unsigned hasCPUID;
		asm (
			// Load EFLAGS into EAX.
			"pushfl;"
			"popl	%%eax;"
			// Save current value.
			"movl	%%eax,%%ebx;"
			// Toggle bit 21.
			"xorl	$0x200000, %%eax;"
			// Load EAX into EFLAGS.
			"pushl	%%eax;"
			"popfl;"
			// Load EFLAGS into EAX.
			"pushfl;"
			"popl	%%eax;"
			// Did bit 21 change?
			"xor	%%ebx, %%eax;"
			"andl	$0x200000, %%eax;"
			: "=a" (hasCPUID) // 0
			: // no input
			: "ebx"
			);
		if (hasCPUID) {
			// Which CPUID calls are supported?
			unsigned highest;
			asm (
				"cpuid;"
				: "=a" (highest) // 0
				: "0" (0) // 1: function
				: "ebx", "ecx", "edx"
				);
			if (highest >= 1) {
				// Get features flags.
				unsigned features;
				asm (
					"cpuid;"
					: "=d" (features) // 0
					: "a" (1) // 1: function
					: "ebx", "ecx"
					);
				fprintf(stderr, "features: %08X\n", features);
				mmxFlag = features & 0x800000;
				bool sseFlag = features & 0x2000000;
				mmxExtFlag = mmxFlag && sseFlag;
			}
		}
	}

	fprintf(stderr, "MMX:              %d\n", mmxFlag);
	fprintf(stderr, "MMX extensions:   %d\n", mmxExtFlag);
}

} // namespace openmsx

/*
int main(char** argv, int argc) {
	openmsx::HostCPU::getInstance();
	return 0;
}
*/
