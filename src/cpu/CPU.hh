// $Id$

#ifndef _CPU_HH_
#define _CPU_HH_

#include <set>
#include "build-info.hh"
#include "openmsx.hh"

namespace openmsx {

class EmuTime;

template <bool bigEndian> struct z80regpair_8bit;
template <> struct z80regpair_8bit<false> { byte l, h; };
template <> struct z80regpair_8bit<true>  { byte h, l; };

/**
 *
 */
class CPU
{
public:
	// cache constants
	static const int CACHE_LINE_BITS = 8;	// 256 bytes
	static const int CACHE_LINE_SIZE = 1 << CACHE_LINE_BITS;
	static const int CACHE_LINE_NUM  = 0x10000 / CACHE_LINE_SIZE;
	static const int CACHE_LINE_LOW  = CACHE_LINE_SIZE - 1;
	static const int CACHE_LINE_HIGH = 0xFFFF - CACHE_LINE_LOW;
	
	// flag positions
	static const byte S_FLAG = 0x80;
	static const byte Z_FLAG = 0x40;
	static const byte Y_FLAG = 0x20;
	static const byte H_FLAG = 0x10;
	static const byte X_FLAG = 0x08;
	static const byte V_FLAG = 0x04;
	static const byte P_FLAG = V_FLAG;
	static const byte N_FLAG = 0x02;
	static const byte C_FLAG = 0x01;
	
	typedef std::multiset<word> BreakPoints;
	
	typedef union {
		z80regpair_8bit<OPENMSX_BIGENDIAN> B;
		word w;
	} z80regpair;

	class CPURegs {
	public:
		z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
		z80regpair AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2;	// refresh = R&127 | R2&128

		void ei() { IFF1 = nextIFF1 = IFF2 = true; }
		void di() { IFF1 = nextIFF1 = IFF2 = false; }
	};
	
	/**
	 * TODO
	 */
	virtual void execute() = 0;
	
	/**
	 * TODO
	 */
	virtual void exitCPULoop() = 0;
	
	/**
	 * Sets the CPU its current time.
	 * This is used to 'warp' a CPU when you switch between Z80/R800.
	 */
	virtual void advance(const EmuTime& time) = 0;

	/**
	 * Returns the CPU its current time.
	 */
	virtual const EmuTime& getCurrentTime() const = 0;

	/**
	 * Wait 
	 */
	virtual void wait(const EmuTime& time) = 0;
	
	/**
	 * Invalidate the CPU its cache for the interval
	 * [start, start+num*CACHE_LINE_SIZE).
	 */
	virtual void invalidateMemCache(word start, unsigned num) = 0;

	/**
	 *
	 */
	virtual CPURegs& getRegisters() = 0;

	/**
	 *
	 */
	virtual void doStep() = 0;
	
	/**
	 *
	 */
	virtual void doContinue() = 0;
	
	/**
	 *
	 */
	virtual void doBreak() = 0;

	/**
	 *
	 */
	void insertBreakPoint(word address);

	/**
	 *
	 */
	void removeBreakPoint(word address);

	/**
	 *
	 */
	const BreakPoints& getBreakPoints() const;
	
protected:
	CPU();
	
	// flag-register tables, initialized at run-time
	static byte ZSTable[256];
	static byte ZSXYTable[256];
	static byte ZSPXYTable[256];
	static byte ZSPTable[256];
	static word DAATable[0x800];
	
	// TODO why exactly are these static?
	// debug variables
	static BreakPoints breakPoints;
	static bool breaked;
	static bool continued;
	static bool step;

	// CPU tracing
	static word start_pc;
};

} // namespace openmsx

#endif // _CPU_HH_
