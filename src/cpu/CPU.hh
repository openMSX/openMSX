// $Id$

#ifndef CPU_HH
#define CPU_HH

#include "BreakPoint.hh"
#include "openmsx.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class EmuTime;
class TclObject;

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

	typedef std::multimap<word, BreakPoint*> BreakPoints;

	class CPURegs {
	public:
		word AF,  BC,  DE,  HL, IX, IY, PC, SP;
		word AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2;	// refresh = R&127 | R2&128

		inline byte getA()   const { return AF >> 8; }
		inline byte getF()   const { return AF & 255; }
		inline byte getB()   const { return BC >> 8; }
		inline byte getC()   const { return BC & 255; }
		inline byte getD()   const { return DE >> 8; }
		inline byte getE()   const { return DE & 255; }
		inline byte getH()   const { return HL >> 8; }
		inline byte getL()   const { return HL & 255; }
		inline byte getIXh() const { return IX >> 8; }
		inline byte getIXl() const { return IX & 255; }
		inline byte getIYh() const { return IY >> 8; }
		inline byte getIYl() const { return IY & 255; }

		inline void setA(byte x)   { AF = (AF & 0x00FF) | (x << 8); }
		inline void setF(byte x)   { AF = (AF & 0xFF00) | x; }
		inline void setB(byte x)   { BC = (BC & 0x00FF) | (x << 8); }
		inline void setC(byte x)   { BC = (BC & 0xFF00) | x; }
		inline void setD(byte x)   { DE = (DE & 0x00FF) | (x << 8); }
		inline void setE(byte x)   { DE = (DE & 0xFF00) | x; }
		inline void setH(byte x)   { HL = (HL & 0x00FF) | (x << 8); }
		inline void setL(byte x)   { HL = (HL & 0xFF00) | x; }
		inline void setIXh(byte x) { IX = (IX & 0x00FF) | (x << 8); }
		inline void setIXl(byte x) { IX = (IX & 0xFF00) | x; }
		inline void setIYh(byte x) { IY = (IY & 0x00FF) | (x << 8); }
		inline void setIYl(byte x) { IY = (IY & 0xFF00) | x; }

		inline void ei() { IFF1 = nextIFF1 = IFF2 = true; }
		inline void di() { IFF1 = nextIFF1 = IFF2 = false; }
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
	 */
	virtual CPURegs& getRegisters() = 0;

	/**
	 */
	virtual void doStep() = 0;

	/**
	 */
	virtual void disasmCommand(const std::vector<TclObject*>& tokens,
	                           TclObject& result) const = 0;

	/**
	 */
	virtual void doContinue() = 0;

	/**
	 */
	virtual void doBreak() = 0;

	/**
	 */
	void insertBreakPoint(std::auto_ptr<BreakPoint> bp);

	/**
	 */
	void removeBreakPoint(const BreakPoint& bp);

	/**
	 */
	const BreakPoints& getBreakPoints() const;

	/**
	 * (un)pause CPU. During pause the CPU executes NOP instructions
	 * continuously (just like during HALT). Used by turbor hw pause.
	 */
	void setPaused(bool paused);
	
protected:
	CPU();
	virtual ~CPU();

	// breakpoint methods used by CPUCore
	inline bool anyBreakPoints() const
	{
		return !breakPoints.empty();
	}
	bool hitBreakPoint(CPURegs& regs) const
	{
		std::pair<BreakPoints::const_iterator,
		          BreakPoints::const_iterator> range =
		                  breakPoints.equal_range(regs.PC);
		for (BreakPoints::const_iterator it = range.first;
		     it != range.second; ++it) {
			if (it->second->isTrue()) {
				return true;
			}
		}
		return false;
	}
	
	// flag-register tables, initialized at run-time
	static byte ZSTable[256];
	static byte ZSXYTable[256];
	static byte ZSPXYTable[256];
	static byte ZSPTable[256];
	static word DAATable[0x800];

	// TODO why exactly are these static?
	// debug variables
	static bool breaked;
	static bool continued;
	static bool step;

	// CPU tracing
	static word start_pc;

	// CPU is paused, used for turbor hw pause
	static bool paused;
	
private:
	static BreakPoints breakPoints;
};

} // namespace openmsx

#endif
