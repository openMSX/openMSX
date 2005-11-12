// $Id$

#ifndef HOSTCPU_HH
#define HOSTCPU_HH

#include "noncopyable.hh"
#include "build-info.hh"

namespace openmsx {

/** Information about the host CPU's capabilities,
  * which are determined at run time.
  * Query capabilities like this:
  *   #ifdef ASM_X86
  *     if (cpu.hasMMX()) { ...inline MMX asm... }
  *   #endif
  * This makes sure instructions for a different CPU family are never fed
  * to the assembler, which may not be able to handle them.
  */
class HostCPU : private noncopyable
{
public:
	/** Get singleton instance.
	  * Note: even though a machine may have multiple CPUs,
	  * they are of the same type (at least for PCs).
	  */
	static HostCPU& getInstance() {
		static HostCPU INSTANCE;
		return INSTANCE;
	}

	/** Does this CPU support the MMX instructions?
	  */
	bool hasMMX() const { return mmxFlag; }

	/** Does this CPU support MMX and the MMX extensions that came with SSE?
	  */
	bool hasMMXEXT() const { return mmxExtFlag; }

	/** Force openMSX to not use any MMX instructions, if available
	  */
	void forceDisableMMX() { mmxFlag = false; mmxExtFlag = false; }

	/** Force openMSX to not use any MMX extensions that came with SSE,
	  * if available
	  */
	void forceDisableMMXEXT() { mmxExtFlag = false; }

private:
	HostCPU();

	bool mmxFlag;
	bool mmxExtFlag;
};

} // namespace openmsx

#endif
