#ifndef CPUREGS_HH
#define CPUREGS_HH

#include "serialize_meta.hh"
#include "openmsx.hh"
#include <bit>
#include <cassert>

namespace openmsx {

template<std::endian> struct z80regpair_8bit;
template<> struct z80regpair_8bit<std::endian::little> { byte l, h; };
template<> struct z80regpair_8bit<std::endian::big   > { byte h, l; };
union z80regpair {
	z80regpair_8bit<std::endian::native> b;
	word w;
};

class CPURegs
{
public:
	explicit CPURegs(bool r800) : HALT_(0), Rmask(r800 ? 0xff : 0x7f) {}
	[[nodiscard]] inline byte getA()   const { return AF_.b.h; }
	[[nodiscard]] inline byte getF()   const { return AF_.b.l; }
	[[nodiscard]] inline byte getB()   const { return BC_.b.h; }
	[[nodiscard]] inline byte getC()   const { return BC_.b.l; }
	[[nodiscard]] inline byte getD()   const { return DE_.b.h; }
	[[nodiscard]] inline byte getE()   const { return DE_.b.l; }
	[[nodiscard]] inline byte getH()   const { return HL_.b.h; }
	[[nodiscard]] inline byte getL()   const { return HL_.b.l; }
	[[nodiscard]] inline byte getA2()  const { return AF2_.b.h; }
	[[nodiscard]] inline byte getF2()  const { return AF2_.b.l; }
	[[nodiscard]] inline byte getB2()  const { return BC2_.b.h; }
	[[nodiscard]] inline byte getC2()  const { return BC2_.b.l; }
	[[nodiscard]] inline byte getD2()  const { return DE2_.b.h; }
	[[nodiscard]] inline byte getE2()  const { return DE2_.b.l; }
	[[nodiscard]] inline byte getH2()  const { return HL2_.b.h; }
	[[nodiscard]] inline byte getL2()  const { return HL2_.b.l; }
	[[nodiscard]] inline byte getIXh() const { return IX_.b.h; }
	[[nodiscard]] inline byte getIXl() const { return IX_.b.l; }
	[[nodiscard]] inline byte getIYh() const { return IY_.b.h; }
	[[nodiscard]] inline byte getIYl() const { return IY_.b.l; }
	[[nodiscard]] inline byte getPCh() const { return PC_.b.h; }
	[[nodiscard]] inline byte getPCl() const { return PC_.b.l; }
	[[nodiscard]] inline byte getSPh() const { return SP_.b.h; }
	[[nodiscard]] inline byte getSPl() const { return SP_.b.l; }

	[[nodiscard]] inline unsigned getAF()  const { return AF_.w; }
	[[nodiscard]] inline unsigned getBC()  const { return BC_.w; }
	[[nodiscard]] inline unsigned getDE()  const { return DE_.w; }
	[[nodiscard]] inline unsigned getHL()  const { return HL_.w; }
	[[nodiscard]] inline unsigned getAF2() const { return AF2_.w; }
	[[nodiscard]] inline unsigned getBC2() const { return BC2_.w; }
	[[nodiscard]] inline unsigned getDE2() const { return DE2_.w; }
	[[nodiscard]] inline unsigned getHL2() const { return HL2_.w; }
	[[nodiscard]] inline unsigned getIX()  const { return IX_.w; }
	[[nodiscard]] inline unsigned getIY()  const { return IY_.w; }
	[[nodiscard]] inline unsigned getPC()  const { return PC_.w; }
	[[nodiscard]] inline unsigned getSP()  const { return SP_.w; }

	[[nodiscard]] inline byte getIM()  const { return IM_; }
	[[nodiscard]] inline byte getI()   const { return I_; }
	[[nodiscard]] inline byte getR()   const { return (R_ & Rmask) | (R2_ & ~Rmask); }
	[[nodiscard]] inline bool getIFF1()     const { return IFF1_; }
	[[nodiscard]] inline bool getIFF2()     const { return IFF2_; }
	[[nodiscard]] inline byte getHALT()     const { return HALT_; }

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

	// Sometimes we need to look at sequences of instructions/actions
	// instead of only individual instructions. The most obvious example is
	// the non-acceptance of IRQs directly after an EI instruction. But
	// also on R800, the timing of the CALL instruction is different when
	// it's directly followed by a POP or RET instruction.
	//
	// The following methods implement this:
	// - setCurrentXXX(): set flag for currently executing instruction
	// - prevWasXXX(): check whether the last executed instruction had
	//                 a specific flag
	// - prev2WasXXX(): same but for the 2nd-to-last instruction
	// - endInstruction(): this shifts the flags of the current instruction
	//                     into the last, last into 2nd-to-last, ...
	//
	// Optimizations: these sequence flags are relatively infrequently
	// needed. So most of the time we want to avoid the (small) overhead of
	// maintaining these flags. (CPU emulation is still to most heavy part
	// of the total emulation, so every cycle we can save counts). Therefor
	// the flags are only shifted in the 'slow' emulation path. This means:
	// - After setting a flag we should enter the slow emulation path for a
	//   few instructions.
	// - Querying the flags should only be done in the slow emulation path.

	// Set EI-flag on current instruction.
	inline void setCurrentEI() {
		prev_ |= 1;
	}
	// Set LDAI-flag on current instruction.
	inline void setCurrentLDAI() {
		prev_ |= 2;
	}
	// Set CALL-flag on current instruction.
	inline void setCurrentCall() {
		prev_ |= 4;
	}
	// Set POPRET-flag on current instruction.
	inline void setCurrentPopRet() {
		prev_ |= 8;
	}

	// Previous instruction was EI?
	[[nodiscard]] inline bool prevWasEI()   const {
		return (prev_ & (1 << 8)) != 0;
	}
	// Previous instruction was LD A,I or LD A,R?  (only set for Z80)
	[[nodiscard]] inline bool prevWasLDAI() const {
		return (prev_ & (2 << 8)) != 0;
	}
	// Previous-previous instruction was a CALL?  (only set for R800)
	[[nodiscard]] inline bool prev2WasCall() const {
		return (prev_ & (4 << (2 * 8))) != 0;
	}
	// Previous instruction was a POP or RET?  (only set for R800)
	[[nodiscard]] inline bool prevWasPopRet() const {
		return (prev_ & (8 << 8)) != 0;
	}

	// Shift flags to the previous instruction positions.
	// Clear flags for current instruction.
	inline void endInstruction() {
		prev_ <<= 8;
	}

	// Clear all previous-flags (called on reset).
	inline void clearPrevious() {
		prev_ = 0;
	}

	// (for debug-only) At the start of an instruction(-block) no flags
	// should be set.
	inline void checkNoCurrentFlags() const {
		// Exception: we do allow a sloppy POP/RET flag, it only needs
		// to be correct after a CALL instruction.
		assert((prev_ & 0xF7) == 0);
	}


	void serialize(Archive auto& ar, unsigned version);

private:
	z80regpair PC_;
	z80regpair AF_, BC_, DE_, HL_;
	z80regpair AF2_, BC2_, DE2_, HL2_;
	z80regpair IX_, IY_, SP_;
	bool IFF1_, IFF2_;
	byte HALT_;
	byte IM_, I_;
	byte R_, R2_; // refresh = R & Rmask | R2 & ~Rmask
	const byte Rmask; // 0x7F for Z80, 0xFF for R800
	unsigned prev_;
};
SERIALIZE_CLASS_VERSION(CPURegs, 3);


/* The above implementation uses a union to access the upper/lower 8 bits in a
 * 16 bit value. At some point in the past I replaced this with the
 * implementation below because it generated faster code. Though when I measure
 * it now, the original version is faster again.
 * TODO need to investigate this further.
 */
#if 0
class CPURegs {
public:
	[[nodiscard]] inline byte getA()   const { return AF >> 8; }
	[[nodiscard]] inline byte getF()   const { return AF & 255; }
	[[nodiscard]] inline byte getB()   const { return BC >> 8; }
	[[nodiscard]] inline byte getC()   const { return BC & 255; }
	[[nodiscard]] inline byte getD()   const { return DE >> 8; }
	[[nodiscard]] inline byte getE()   const { return DE & 255; }
	[[nodiscard]] inline byte getH()   const { return HL >> 8; }
	[[nodiscard]] inline byte getL()   const { return HL & 255; }
	[[nodiscard]] inline byte getA2()  const { return AF2 >> 8; }
	[[nodiscard]] inline byte getF2()  const { return AF2 & 255; }
	[[nodiscard]] inline byte getB2()  const { return BC2 >> 8; }
	[[nodiscard]] inline byte getC2()  const { return BC2 & 255; }
	[[nodiscard]] inline byte getD2()  const { return DE2 >> 8; }
	[[nodiscard]] inline byte getE2()  const { return DE2 & 255; }
	[[nodiscard]] inline byte getH2()  const { return HL2 >> 8; }
	[[nodiscard]] inline byte getL2()  const { return HL2 & 255; }
	[[nodiscard]] inline byte getIXh() const { return IX >> 8; }
	[[nodiscard]] inline byte getIXl() const { return IX & 255; }
	[[nodiscard]] inline byte getIYh() const { return IY >> 8; }
	[[nodiscard]] inline byte getIYl() const { return IY & 255; }
	[[nodiscard]] inline byte getPCh() const { return PC >> 8; }
	[[nodiscard]] inline byte getPCl() const { return PC & 255; }
	[[nodiscard]] inline byte getSPh() const { return SP >> 8; }
	[[nodiscard]] inline byte getSPl() const { return SP & 255; }
	[[nodiscard]] inline word getAF()  const { return AF; }
	[[nodiscard]] inline word getBC()  const { return BC; }
	[[nodiscard]] inline word getDE()  const { return DE; }
	[[nodiscard]] inline word getHL()  const { return HL; }
	[[nodiscard]] inline word getAF2() const { return AF2; }
	[[nodiscard]] inline word getBC2() const { return BC2; }
	[[nodiscard]] inline word getDE2() const { return DE2; }
	[[nodiscard]] inline word getHL2() const { return HL2; }
	[[nodiscard]] inline word getIX()  const { return IX; }
	[[nodiscard]] inline word getIY()  const { return IY; }
	[[nodiscard]] inline word getPC()  const { return PC; }
	[[nodiscard]] inline word getSP()  const { return SP; }
	[[nodiscard]] inline byte getIM()  const { return IM; }
	[[nodiscard]] inline byte getI()   const { return I; }
	[[nodiscard]] inline byte getR()   const { return (R & 0x7F) | (R2 & 0x80); }
	[[nodiscard]] inline bool getIFF1()     const { return IFF1; }
	[[nodiscard]] inline bool getIFF2()     const { return IFF2; }
	[[nodiscard]] inline bool getHALT()     const { return HALT; }

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
