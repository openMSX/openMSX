// $Id$

#ifndef CPU_HH
#define CPU_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include "build-info.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class EmuTime;
class TclObject;
class BreakPoint;

template <bool bigEndian> struct z80regpair_8bit;
template <> struct z80regpair_8bit<false> { byte l, h; };
template <> struct z80regpair_8bit<true>  { byte h, l; };
typedef union {
	z80regpair_8bit<OPENMSX_BIGENDIAN> b;
	word w;
} z80regpair;

class CPU : private noncopyable
{
public:
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

/*
 * Below are two different implementations for the CPURegs class:
 *   1) use arithmetic to extract the upper/lower byte from a word
 *   2) use a union to directly access the bytes in the word
 * At some time in the past I replaced 2) with 1) because 1) was faster,
 * but when I measure it now again 2) is faster.
 * TODO need to investigate this further.
 */
#if 0
	class CPURegs {
	public:
		inline byte getA()   const { return AF >> 8; }
		inline byte getF()   const { return AF & 255; }
		inline byte getB()   const { return BC >> 8; }
		inline byte getC()   const { return BC & 255; }
		inline byte getD()   const { return DE >> 8; }
		inline byte getE()   const { return DE & 255; }
		inline byte getH()   const { return HL >> 8; }
		inline byte getL()   const { return HL & 255; }
		inline byte getA2()  const { return AF2 >> 8; }
		inline byte getF2()  const { return AF2 & 255; }
		inline byte getB2()  const { return BC2 >> 8; }
		inline byte getC2()  const { return BC2 & 255; }
		inline byte getD2()  const { return DE2 >> 8; }
		inline byte getE2()  const { return DE2 & 255; }
		inline byte getH2()  const { return HL2 >> 8; }
		inline byte getL2()  const { return HL2 & 255; }
		inline byte getIXh() const { return IX >> 8; }
		inline byte getIXl() const { return IX & 255; }
		inline byte getIYh() const { return IY >> 8; }
		inline byte getIYl() const { return IY & 255; }
		inline byte getPCh() const { return PC >> 8; }
		inline byte getPCl() const { return PC & 255; }
		inline byte getSPh() const { return SP >> 8; }
		inline byte getSPl() const { return SP & 255; }
		inline word getAF()  const { return AF; }
		inline word getBC()  const { return BC; }
		inline word getDE()  const { return DE; }
		inline word getHL()  const { return HL; }
		inline word getAF2() const { return AF2; }
		inline word getBC2() const { return BC2; }
		inline word getDE2() const { return DE2; }
		inline word getHL2() const { return HL2; }
		inline word getIX()  const { return IX; }
		inline word getIY()  const { return IY; }
		inline word getPC()  const { return PC; }
		inline word getSP()  const { return SP; }
		inline byte getIM()  const { return IM; }
		inline byte getI()   const { return I; }
		inline byte getR()   const { return (R & 0x7F) | (R2 & 0x80); }
		inline bool getIFF1()     const { return IFF1; }
		inline bool getIFF2()     const { return IFF2; }
		inline bool getHALT()     const { return HALT; }
		inline bool getNextIFF1() const { return nextIFF1; }

		inline void setA(byte x)   { AF = (AF & 0x00FF) | (x << 8); }
		inline void setF(byte x)   { AF = (AF & 0xFF00) | x; }
		inline void setB(byte x)   { BC = (BC & 0x00FF) | (x << 8); }
		inline void setC(byte x)   { BC = (BC & 0xFF00) | x; }
		inline void setD(byte x)   { DE = (DE & 0x00FF) | (x << 8); }
		inline void setE(byte x)   { DE = (DE & 0xFF00) | x; }
		inline void setH(byte x)   { HL = (HL & 0x00FF) | (x << 8); }
		inline void setL(byte x)   { HL = (HL & 0xFF00) | x; }
		inline void setA2(byte x)  { AF2 = (AF2 & 0x00FF) | (x << 8); }
		inline void setF2(byte x)  { AF2 = (AF2 & 0xFF00) | x; }
		inline void setB2(byte x)  { BC2 = (BC2 & 0x00FF) | (x << 8); }
		inline void setC2(byte x)  { BC2 = (BC2 & 0xFF00) | x; }
		inline void setD2(byte x)  { DE2 = (DE2 & 0x00FF) | (x << 8); }
		inline void setE2(byte x)  { DE2 = (DE2 & 0xFF00) | x; }
		inline void setH2(byte x)  { HL2 = (HL2 & 0x00FF) | (x << 8); }
		inline void setL2(byte x)  { HL2 = (HL2 & 0xFF00) | x; }
		inline void setIXh(byte x) { IX = (IX & 0x00FF) | (x << 8); }
		inline void setIXl(byte x) { IX = (IX & 0xFF00) | x; }
		inline void setIYh(byte x) { IY = (IY & 0x00FF) | (x << 8); }
		inline void setIYl(byte x) { IY = (IY & 0xFF00) | x; }
		inline void setPCh(byte x) { PC = (PC & 0x00FF) | (x << 8); }
		inline void setPCl(byte x) { PC = (PC & 0xFF00) | x; }
		inline void setSPh(byte x) { SP = (SP & 0x00FF) | (x << 8); }
		inline void setSPl(byte x) { SP = (SP & 0xFF00) | x; }
		inline void setAF(word x)  { AF = x; }
		inline void setBC(word x)  { BC = x; }
		inline void setDE(word x)  { DE = x; }
		inline void setHL(word x)  { HL = x; }
		inline void setAF2(word x) { AF2 = x; }
		inline void setBC2(word x) { BC2 = x; }
		inline void setDE2(word x) { DE2 = x; }
		inline void setHL2(word x) { HL2 = x; }
		inline void setIX(word x)  { IX = x; }
		inline void setIY(word x)  { IY = x; }
		inline void setPC(word x)  { PC = x; }
		inline void setSP(word x)  { SP = x; }
		inline void setIM(byte x)  { IM = x; }
		inline void setI(byte x)   { I = x; }
		inline void setR(byte x)   { R = x; R2 = x; }
		inline void setIFF1(bool x)     { IFF1 = x; }
		inline void setIFF2(bool x)     { IFF2 = x; }
		inline void setHALT(bool x)     { HALT = x; }
		inline void setNextIFF1(bool x) { nextIFF1 = x; }

		inline void incR(byte x) { R += x; }

		inline void ei() {
			IFF1     = true;
			IFF2     = true;
			nextIFF1 = true;
		}
		inline void di() {
			IFF1     = false;
			IFF2     = false;
			nextIFF1 = false;
		}
	private:
		word AF, BC, DE, HL;
		word AF2, BC2, DE2, HL2;
		word IX, IY, PC, SP;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2; // refresh = R&127 | R2&128
	};
#else
	class CPURegs {
	public:
		inline byte getA()   const { return AF.b.h; }
		inline byte getF()   const { return AF.b.l; }
		inline byte getB()   const { return BC.b.h; }
		inline byte getC()   const { return BC.b.l; }
		inline byte getD()   const { return DE.b.h; }
		inline byte getE()   const { return DE.b.l; }
		inline byte getH()   const { return HL.b.h; }
		inline byte getL()   const { return HL.b.l; }
		inline byte getA2()  const { return AF2.b.h; }
		inline byte getF2()  const { return AF2.b.l; }
		inline byte getB2()  const { return BC2.b.h; }
		inline byte getC2()  const { return BC2.b.l; }
		inline byte getD2()  const { return DE2.b.h; }
		inline byte getE2()  const { return DE2.b.l; }
		inline byte getH2()  const { return HL2.b.h; }
		inline byte getL2()  const { return HL2.b.l; }
		inline byte getIXh() const { return IX.b.h; }
		inline byte getIXl() const { return IX.b.l; }
		inline byte getIYh() const { return IY.b.h; }
		inline byte getIYl() const { return IY.b.l; }
		inline byte getPCh() const { return PC.b.h; }
		inline byte getPCl() const { return PC.b.l; }
		inline byte getSPh() const { return SP.b.h; }
		inline byte getSPl() const { return SP.b.l; }
		inline word getAF()  const { return AF.w; }
		inline word getBC()  const { return BC.w; }
		inline word getDE()  const { return DE.w; }
		inline word getHL()  const { return HL.w; }
		inline word getAF2() const { return AF2.w; }
		inline word getBC2() const { return BC2.w; }
		inline word getDE2() const { return DE2.w; }
		inline word getHL2() const { return HL2.w; }
		inline word getIX()  const { return IX.w; }
		inline word getIY()  const { return IY.w; }
		inline word getPC()  const { return PC.w; }
		inline word getSP()  const { return SP.w; }
		inline byte getIM()  const { return IM; }
		inline byte getI()   const { return I; }
		inline byte getR()   const { return (R & 0x7F) | (R2 & 0x80); }
		inline bool getIFF1()     const { return IFF1; }
		inline bool getIFF2()     const { return IFF2; }
		inline bool getHALT()     const { return HALT; }
		inline bool getNextIFF1() const { return nextIFF1; }

		inline void setA(byte x)   { AF.b.h = x; }
		inline void setF(byte x)   { AF.b.l = x; }
		inline void setB(byte x)   { BC.b.h = x; }
		inline void setC(byte x)   { BC.b.l = x; }
		inline void setD(byte x)   { DE.b.h = x; }
		inline void setE(byte x)   { DE.b.l = x; }
		inline void setH(byte x)   { HL.b.h = x; }
		inline void setL(byte x)   { HL.b.l = x; }
		inline void setA2(byte x)  { AF2.b.h = x; }
		inline void setF2(byte x)  { AF2.b.l = x; }
		inline void setB2(byte x)  { BC2.b.h = x; }
		inline void setC2(byte x)  { BC2.b.l = x; }
		inline void setD2(byte x)  { DE2.b.h = x; }
		inline void setE2(byte x)  { DE2.b.l = x; }
		inline void setH2(byte x)  { HL2.b.h = x; }
		inline void setL2(byte x)  { HL2.b.l = x; }
		inline void setIXh(byte x) { IX.b.h = x; }
		inline void setIXl(byte x) { IX.b.l = x; }
		inline void setIYh(byte x) { IY.b.h = x; }
		inline void setIYl(byte x) { IY.b.l = x; }
		inline void setPCh(byte x) { PC.b.h = x; }
		inline void setPCl(byte x) { PC.b.l = x; }
		inline void setSPh(byte x) { SP.b.h = x; }
		inline void setSPl(byte x) { SP.b.l = x; }
		inline void setAF(word x)  { AF.w = x; }
		inline void setBC(word x)  { BC.w = x; }
		inline void setDE(word x)  { DE.w = x; }
		inline void setHL(word x)  { HL.w = x; }
		inline void setAF2(word x) { AF2.w = x; }
		inline void setBC2(word x) { BC2.w = x; }
		inline void setDE2(word x) { DE2.w = x; }
		inline void setHL2(word x) { HL2.w = x; }
		inline void setIX(word x)  { IX.w = x; }
		inline void setIY(word x)  { IY.w = x; }
		inline void setPC(word x)  { PC.w = x; }
		inline void setSP(word x)  { SP.w = x; }
		inline void setIM(byte x)  { IM = x; }
		inline void setI(byte x)   { I = x; }
		inline void setR(byte x)   { R = x; R2 = x; }
		inline void setIFF1(bool x)     { IFF1 = x; }
		inline void setIFF2(bool x)     { IFF2 = x; }
		inline void setHALT(bool x)     { HALT = x; }
		inline void setNextIFF1(bool x) { nextIFF1 = x; }

		inline void incR(byte x) { R += x; }

		inline void ei() {
			IFF1     = true;
			IFF2     = true;
			nextIFF1 = true;
		}
		inline void di() {
			IFF1     = false;
			IFF2     = false;
			nextIFF1 = false;
		}
	private:
		z80regpair AF, BC, DE, HL;
		z80regpair AF2, BC2, DE2, HL2;
		z80regpair IX, IY, PC, SP;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2; // refresh = R&127 | R2&128
	};
#endif

	/**
	 * TODO
	 */
	virtual void execute() = 0;

	/** Request to exit the main CPU emulation loop.
	  * This method may only be called from the main thread. The CPU loop
	  * will immediately be exited (current instruction will be finished,
	  * but no new instruction will be executed).
	  */
	virtual void exitCPULoopSync() = 0;

	/** Similar to exitCPULoopSync(), but this method may be called from
	  * any thread. Although now the loop will only be exited 'soon'.
	  */
	virtual void exitCPULoopAsync() = 0;

	/**
	 * Sets the CPU its current time.
	 * This is used to 'warp' a CPU when you switch between Z80/R800.
	 */
	virtual void warp(const EmuTime& time) = 0;

	/**
	 * Returns the CPU its current time.
	 */
	virtual const EmuTime& getCurrentTime() const = 0;

	/**
	 * Wait
	 */
	virtual void wait(const EmuTime& time) = 0;

	/** Inform CPU of new (possibly earlier) sync point.
	 */
	virtual void setNextSyncPoint(const EmuTime& time) = 0;

	/**
	 * Invalidate the CPU its cache for the interval
	 * [start, start+num*CacheLine::SIZE).
	 */
	virtual void invalidateMemCache(unsigned start, unsigned num) = 0;

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
	bool isBreaked() const;

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
	bool checkBreakPoints(CPURegs& regs) const
	{
		std::pair<BreakPoints::const_iterator,
		          BreakPoints::const_iterator> range =
		                  breakPoints.equal_range(regs.getPC());
		if (range.first == range.second) {
			return false;
		}
		// slow path non-inlined
		checkBreakPoints(range);
		return true;
	}

	// flag-register tables, initialized at run-time
	static byte ZSTable[256];
	static byte ZSXYTable[256];
	static byte ZSPXYTable[256];
	static byte ZSPTable[256];
	static word DAATable[0x800];
	static const byte ZS0     = Z_FLAG;
	static const byte ZSXY0   = Z_FLAG;
	static const byte ZSXY255 = S_FLAG | X_FLAG | Y_FLAG;
	static const byte ZSPXY0  = Z_FLAG | V_FLAG;

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
	void checkBreakPoints(std::pair<BreakPoints::const_iterator,
	                              BreakPoints::const_iterator> range) const;

	static BreakPoints breakPoints;
};

} // namespace openmsx

#endif
