// $Id$

#ifndef HOSTCPU_HH
#define HOSTCPU_HH

#include "build-info.hh"

/** Information about the host CPU's capabilities,
  * which are determined at run time.
  * Query capabilities like this:<pre>
  *   \#if ASM_X86
  *     if (HostCPU::hasMMX()) { ...inline MMX asm... }
  *   \#endif</pre>
  * This makes sure instructions for a different CPU family are never fed
  * to the assembler, which may not be able to handle them.
  */
namespace HostCPU {

	extern bool mmxFlag;
	extern bool sseFlag;
	extern bool sse2Flag;

	/** This function should be called before the  hasXXX()  functions,
	  *  (if not the hasXXX() function returns false).
	  * This function  MUST  be called before the forceXXX() functions.
	  */
	void init();


	/** Does this CPU support the MMX instructions?
	  */
	inline bool hasMMX() { return mmxFlag; }

	/** Does this CPU support the SSE instructions?
	  */
	inline bool hasSSE() { return sseFlag; }

	/** Does this CPU support the SSE2 instructions?
	  */
	inline bool hasSSE2() { return sse2Flag; }


	/** Force openMSX to not use any SSE2 instructions, if available
	  */
	inline void forceDisableSSE2() { sse2Flag = false; }

	/** Force openMSX to not use any SSE (or MMX extensions), if available
	  */
	inline void forceDisableSSE() { sseFlag = false; forceDisableSSE2(); }

	/** Force openMSX to not use any MMX instructions, if available
	  */
	inline void forceDisableMMX() { mmxFlag = false; forceDisableSSE(); }

} // namespace HostCPU

#endif
