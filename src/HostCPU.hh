// $Id$

#ifndef __HOSTCPU_HH__
#define __HOSTCPU_HH__

#include "build-info.hh"


namespace openmsx {

class HostCPU {
public:
	/** Get singleton instance.
	  * Note: even though a machine may have multiple CPUs,
	  * they are of the same type (at least for PCs).
	  */
	static HostCPU& getInstance() {
		static HostCPU INSTANCE;
		return INSTANCE;
	}

	/* Note:
	These methods must be inline and check the CPU family, so that they return
	false at compile time if the family does not match. That way, we won't
	generate code that will never be used, or could cause compile errors it
	the assembler for another CPU family is not available.
	*/

	/** Does this CPU support the MMX instructions?
	  */
	bool hasMMX() const {
		return ASM_X86 && mmxFlag;
	}

	/** Does this CPU support MMX and the MMX extensions that came with SSE?
	  */
	bool hasMMXEXT() const {
		return ASM_X86 && mmxExtFlag;
	}

	/** Does this CPU support an impossible feature?
	  * No CPU does; this method is for testing conditional compilation.
	  */
	bool hasImpossible() const {
		return false;
	}

private:
	HostCPU();
	bool mmxFlag;
	bool mmxExtFlag;
};

} // namespace openmsx

#endif //__HOSTCPU_HH__
