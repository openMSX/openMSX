#ifndef CPUREGS_HH
#define CPUREGS_HH

#include "serialize_meta.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

template<bool BigEndian> struct z80regpair_8bit;
template<> struct z80regpair_8bit<false> { byte l, h; };
template<> struct z80regpair_8bit<true>  { byte h, l; };
union z80regpair {
	z80regpair_8bit<OPENMSX_BIGENDIAN> b;
	word w;
};

class CPURegs
{
public:
	CPURegs(bool r800) : HALT_(0), Rmask(r800 ? 0xff : 0x7f) {}
	inline byte getA()   const { return AF_.b.h; }
	inline byte getF()   const { return AF_.b.l; }
	inline byte getB()   const { return BC_.b.h; }
	inline byte getC()   const { return BC_.b.l; }
	inline byte getD()   const { return DE_.b.h; }
	inline byte getE()   const { return DE_.b.l; }
	inline byte getH()   const { return HL_.b.h; }
	inline byte getL()   const { return HL_.b.l; }
	inline byte getA2()  const { return AF2_.b.h; }
	inline byte getF2()  const { return AF2_.b.l; }
	inline byte getB2()  const { return BC2_.b.h; }
	inline byte getC2()  const { return BC2_.b.l; }
	inline byte getD2()  const { return DE2_.b.h; }
	inline byte getE2()  const { return DE2_.b.l; }
	inline byte getH2()  const { return HL2_.b.h; }
	inline byte getL2()  const { return HL2_.b.l; }
	inline byte getIXh() const { return IX_.b.h; }
	inline byte getIXl() const { return IX_.b.l; }
	inline byte getIYh() const { return IY_.b.h; }
	inline byte getIYl() const { return IY_.b.l; }
	inline byte getPCh() const { return PC_.b.h; }
	inline byte getPCl() const { return PC_.b.l; }
	inline byte getSPh() const { return SP_.b.h; }
	inline byte getSPl() const { return SP_.b.l; }

	inline unsigned getAF()  const { return AF_.w; }
	inline unsigned getBC()  const { return BC_.w; }
	inline unsigned getDE()  const { return DE_.w; }
	inline unsigned getHL()  const { return HL_.w; }
	inline unsigned getAF2() const { return AF2_.w; }
	inline unsigned getBC2() const { return BC2_.w; }
	inline unsigned getDE2() const { return DE2_.w; }
	inline unsigned getHL2() const { return HL2_.w; }
	inline unsigned getIX()  const { return IX_.w; }
	inline unsigned getIY()  const { return IY_.w; }
	inline unsigned getPC()  const { return PC_.w; }
	inline unsigned getSP()  const { return SP_.w; }

	inline byte getIM()  const { return IM_; }
	inline byte getI()   const { return I_; }
	inline byte getR()   const { return (R_ & Rmask) | (R2_ & ~Rmask); }
	inline bool getIFF1()     const { return IFF1_; }
	inline bool getIFF2()     const { return IFF2_; }
	inline byte getHALT()     const { return HALT_; }

	inline void setA(byte x)   { AF_.b.h = x; }
	inline void setF(byte x)   { AF_.b.l = x; }
	inline void setB(byte x)   { BC_.b.h = x; }
	inline void setC(byte x)   { BC_.b.l = x; }
	inline void setD(byte x)   { DE_.b.h = x; }
	inline void setE(byte x)   { DE_.b.l = x; }
	inline void setH(byte x)   { HL_.b.h = x; }
	inline void setL(byte x)   { HL_.b.l = x; }
	inline void setA2(byte x)  { AF2_.b.h = x; }
	inline void setF2(byte x)  { AF2_.b.l = x; }
	inline void setB2(byte x)  { BC2_.b.h = x; }
	inline void setC2(byte x)  { BC2_.b.l = x; }
	inline void setD2(byte x)  { DE2_.b.h = x; }
	inline void setE2(byte x)  { DE2_.b.l = x; }
	inline void setH2(byte x)  { HL2_.b.h = x; }
	inline void setL2(byte x)  { HL2_.b.l = x; }
	inline void setIXh(byte x) { IX_.b.h = x; }
	inline void setIXl(byte x) { IX_.b.l = x; }
	inline void setIYh(byte x) { IY_.b.h = x; }
	inline void setIYl(byte x) { IY_.b.l = x; }
	inline void setPCh(byte x) { PC_.b.h = x; }
	inline void setPCl(byte x) { PC_.b.l = x; }
	inline void setSPh(byte x) { SP_.b.h = x; }
	inline void setSPl(byte x) { SP_.b.l = x; }

	inline void setAF(unsigned x)  { AF_.w = x; }
	inline void setBC(unsigned x)  { BC_.w = x; }
	inline void setDE(unsigned x)  { DE_.w = x; }
	inline void setHL(unsigned x)  { HL_.w = x; }
	inline void setAF2(unsigned x) { AF2_.w = x; }
	inline void setBC2(unsigned x) { BC2_.w = x; }
	inline void setDE2(unsigned x) { DE2_.w = x; }
	inline void setHL2(unsigned x) { HL2_.w = x; }
	inline void setIX(unsigned x)  { IX_.w = x; }
	inline void setIY(unsigned x)  { IY_.w = x; }
	inline void setPC(unsigned x)  { PC_.w = x; }
	inline void setSP(unsigned x)  { SP_.w = x; }

	inline void setIM(byte x) { IM_ = x; }
	inline void setI(byte x)  { I_ = x; }
	inline void setR(byte x)  { R_ = x; R2_ = x; }
	inline void setIFF1(bool x)    { IFF1_ = x; }
	inline void setIFF2(bool x)    { IFF2_ = x; }
	inline void setHALT(bool x)    { HALT_ = (HALT_ & ~1) | (x ? 1 : 0); }
	inline void setExtHALT(bool x) { HALT_ = (HALT_ & ~2) | (x ? 2 : 0); }

	inline void incR(byte x) { R_ += x; }

	// These methods are used to set/query whether the previously
	// executed instruction was a 'EI' or 'LD A,{I,R}' instruction.
	// Initially this could only be queried between two
	// instructions, so e.g. after the EI instruction was executed
	// but before the next one has started, for emulation this is
	// good enough. But for debugging we still want to be able to
	// query this info during the execution of the next
	// instruction: e.g. a IO-watchpoint is triggered during the
	// execution of some OUT instruction, at the time we evaluate
	// the condition for that watchpoint, we still want to be able
	// to query whether the previous instruction was a EI
	// instruction.
	inline bool isSameAfter() const {
		// Between two instructions these two should be the same
		return after_ == afterNext_;
	}
	inline bool getAfterEI()   const {
		assert(isSameAfter());
		return (after_ & 0x01) != 0;
	}
	inline bool getAfterLDAI() const {
		assert(isSameAfter());
		return (after_ & 0x02) != 0;
	}
	inline bool debugGetAfterEI() const {
		// Can be called during execution of an instruction
		return (after_ & 0x01) != 0;
	}
	inline void clearNextAfter() {
		// Right before executing an instruction this should be
		// cleared
		afterNext_ = 0x00;
	}
	inline bool isNextAfterClear() const {
		// In the fast code path we avoid calling clearNextAfter()
		// before every instruction. But in debug mode we want
		// to verify that this optimzation is valid.
		return afterNext_ == 0;
	}
	inline void setAfterEI() {
		// Set both after_ and afterNext_. Can only be called
		// at the end of an instruction (status of prev
		// instruction can't be queried anymore)
		assert(isNextAfterClear());
		afterNext_ = after_ = 0x01;
	}
	inline void setAfterLDAI() {
		// Set both, see above.
		assert(isNextAfterClear());
		afterNext_ = after_ = 0x02;
	}
	inline void copyNextAfter() {
		// At the end of an instruction, the next flags become
		// the current flags. setAfterEI/LDAI() already sets
		// both after_ and afterNext_, thus calling this method
		// is only required to clear the flags. Instructions
		// right after a EI or LD A,I/R instruction are always
		// executed in the slow code path. So this means that
		// in the fast code path we don't need to call
		// copyNextAfter().
		after_ = afterNext_;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	z80regpair PC_;
	z80regpair AF_, BC_, DE_, HL_;
	z80regpair AF2_, BC2_, DE2_, HL2_;
	z80regpair IX_, IY_, SP_;
	bool IFF1_, IFF2_;
	byte after_, afterNext_;
	byte HALT_;
	byte IM_, I_;
	byte R_, R2_; // refresh = R & Rmask | R2 & ~Rmask
	const byte Rmask; // 0x7F for Z80, 0xFF for R800
};
SERIALIZE_CLASS_VERSION(CPURegs, 2);


/* The above implementation uses a union to access the upper/lower 8 bits in a
 * 16 bit value. At some point in the past I replaced this with the
 * implementation below because it generated faster code. Though when I measure
 * it now, the original version is faster again.
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

	inline void incR(byte x) { R += x; }

private:
	word AF, BC, DE, HL;
	word AF2, BC2, DE2, HL2;
	word IX, IY, PC, SP;
	bool IFF1, IFF2, HALT;
	byte IM, I;
	byte R, R2; // refresh = R&127 | R2&128
};
#endif

} // namespace openmsx

#endif
