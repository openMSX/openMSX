// $Id$

#ifndef HOSTCPU_HH
#define HOSTCPU_HH

#include "noncopyable.hh"
#include "build-info.hh"

namespace openmsx {

/** Information about the host CPU's capabilities,
  * which are determined at run time.
  * Query capabilities like this:<pre>
  *   \#ifdef ASM_X86
  *     if (cpu.hasMMX()) { ...inline MMX asm... }
  *   \#endif</pre>
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

	/** Does this CPU support the SSE instructions?
	  */
	bool hasSSE() const { return sseFlag; }

	/** Does this CPU support the SSE2 instructions?
	  */
	bool hasSSE2() const { return sse2Flag; }

	/** Force openMSX to not use any MMX instructions, if available
	  */
	void forceDisableMMX() { mmxFlag = false; forceDisableSSE(); }

	/** Force openMSX to not use any SSE (or MMX extensions), if available
	  */
	void forceDisableSSE() { sseFlag = false; forceDisableSSE2(); }

	/** Force openMSX to not use any SSE2 instructions, if available
	  */
	void forceDisableSSE2() { sse2Flag = false; }

private:
	HostCPU();

	bool mmxFlag;
	bool sseFlag;
	bool sse2Flag;
};

} // namespace openmsx

#endif
