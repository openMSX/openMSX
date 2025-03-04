#ifndef CPUREGS_HH
#define CPUREGS_HH

#include "serialize_meta.hh"
#include "openmsx.hh"

#include <bit>
#include <cassert>

namespace openmsx {

template<std::endian> struct z80regPair_8bit;
template<> struct z80regPair_8bit<std::endian::little> { byte l, h; };
template<> struct z80regPair_8bit<std::endian::big   > { byte h, l; };
union z80regPair {
	z80regPair_8bit<std::endian::native> b;
	word w;
};

class CPURegs
{
public:
	explicit CPURegs(bool r800) : Rmask(r800 ? 0xff : 0x7f) {}
	[[nodiscard]] byte getA()   const { return AF_.b.h; }
	[[nodiscard]] byte getF()   const { return AF_.b.l; }
	[[nodiscard]] byte getB()   const { return BC_.b.h; }
	[[nodiscard]] byte getC()   const { return BC_.b.l; }
	[[nodiscard]] byte getD()   const { return DE_.b.h; }
	[[nodiscard]] byte getE()   const { return DE_.b.l; }
	[[nodiscard]] byte getH()   const { return HL_.b.h; }
	[[nodiscard]] byte getL()   const { return HL_.b.l; }
	[[nodiscard]] byte getA2()  const { return AF2_.b.h; }
	[[nodiscard]] byte getF2()  const { return AF2_.b.l; }
	[[nodiscard]] byte getB2()  const { return BC2_.b.h; }
	[[nodiscard]] byte getC2()  const { return BC2_.b.l; }
	[[nodiscard]] byte getD2()  const { return DE2_.b.h; }
	[[nodiscard]] byte getE2()  const { return DE2_.b.l; }
	[[nodiscard]] byte getH2()  const { return HL2_.b.h; }
	[[nodiscard]] byte getL2()  const { return HL2_.b.l; }
	[[nodiscard]] byte getIXh() const { return IX_.b.h; }
	[[nodiscard]] byte getIXl() const { return IX_.b.l; }
	[[nodiscard]] byte getIYh() const { return IY_.b.h; }
	[[nodiscard]] byte getIYl() const { return IY_.b.l; }
	[[nodiscard]] byte getPCh() const { return PC_.b.h; }
	[[nodiscard]] byte getPCl() const { return PC_.b.l; }
	[[nodiscard]] byte getSPh() const { return SP_.b.h; }
	[[nodiscard]] byte getSPl() const { return SP_.b.l; }

	[[nodiscard]] word getAF()  const { return AF_.w; }
	[[nodiscard]] word getBC()  const { return BC_.w; }
	[[nodiscard]] word getDE()  const { return DE_.w; }
	[[nodiscard]] word getHL()  const { return HL_.w; }
	[[nodiscard]] word getAF2() const { return AF2_.w; }
	[[nodiscard]] word getBC2() const { return BC2_.w; }
	[[nodiscard]] word getDE2() const { return DE2_.w; }
	[[nodiscard]] word getHL2() const { return HL2_.w; }
	[[nodiscard]] word getIX()  const { return IX_.w; }
	[[nodiscard]] word getIY()  const { return IY_.w; }
	[[nodiscard]] word getPC()  const { return PC_.w; }
	[[nodiscard]] word getSP()  const { return SP_.w; }

	[[nodiscard]] byte getIM()  const { return IM_; }
	[[nodiscard]] byte getI()   const { return I_; }
	[[nodiscard]] byte getR()   const { return (R_ & Rmask) | (R2_ & ~Rmask); }
	[[nodiscard]] bool getIFF1()     const { return IFF1_; }
	[[nodiscard]] bool getIFF2()     const { return IFF2_; }
	[[nodiscard]] byte getHALT()     const { return HALT_; }

	void setA(byte x)   { AF_.b.h = x; }
	void setF(byte x)   { AF_.b.l = x; }
	void setB(byte x)   { BC_.b.h = x; }
	void setC(byte x)   { BC_.b.l = x; }
	void setD(byte x)   { DE_.b.h = x; }
	void setE(byte x)   { DE_.b.l = x; }
	void setH(byte x)   { HL_.b.h = x; }
	void setL(byte x)   { HL_.b.l = x; }
	void setA2(byte x)  { AF2_.b.h = x; }
	void setF2(byte x)  { AF2_.b.l = x; }
	void setB2(byte x)  { BC2_.b.h = x; }
	void setC2(byte x)  { BC2_.b.l = x; }
	void setD2(byte x)  { DE2_.b.h = x; }
	void setE2(byte x)  { DE2_.b.l = x; }
	void setH2(byte x)  { HL2_.b.h = x; }
	void setL2(byte x)  { HL2_.b.l = x; }
	void setIXh(byte x) { IX_.b.h = x; }
	void setIXl(byte x) { IX_.b.l = x; }
	void setIYh(byte x) { IY_.b.h = x; }
	void setIYl(byte x) { IY_.b.l = x; }
	void setPCh(byte x) { PC_.b.h = x; }
	void setPCl(byte x) { PC_.b.l = x; }
	void setSPh(byte x) { SP_.b.h = x; }
	void setSPl(byte x) { SP_.b.l = x; }

	void setAF(word x)  { AF_.w = x; }
	void setBC(word x)  { BC_.w = x; }
	void setDE(word x)  { DE_.w = x; }
	void setHL(word x)  { HL_.w = x; }
	void setAF2(word x) { AF2_.w = x; }
	void setBC2(word x) { BC2_.w = x; }
	void setDE2(word x) { DE2_.w = x; }
	void setHL2(word x) { HL2_.w = x; }
	void setIX(word x)  { IX_.w = x; }
	void setIY(word x)  { IY_.w = x; }
	void setPC(word x)  { PC_.w = x; }
	void setSP(word x)  { SP_.w = x; }

	void setIM(byte x) { IM_ = x; }
	void setI(byte x)  { I_ = x; }
	void setR(byte x)  { R_ = x; R2_ = x; }
	void setIFF1(bool x)    { IFF1_ = x; }
	void setIFF2(bool x)    { IFF2_ = x; }
	void setHALT(bool x)    { HALT_ = (HALT_ & ~1) | (x ? 1 : 0); }
	void setExtHALT(bool x) { HALT_ = (HALT_ & ~2) | (x ? 2 : 0); }

	void incR(byte x) { R_ += x; }

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
	void setCurrentEI() {
		prev_ |= 1;
	}
	// Set LDAI-flag on current instruction.
	void setCurrentLDAI() {
		prev_ |= 2;
	}
	// Set CALL-flag on current instruction.
	void setCurrentCall() {
		prev_ |= 4;
	}
	// Set POP-RET-flag on current instruction.
	void setCurrentPopRet() {
		prev_ |= 8;
	}

	// Previous instruction was EI?
	[[nodiscard]] bool prevWasEI()   const {
		return (prev_ & (1 << 8)) != 0;
	}
	// Previous instruction was LD A,I or LD A,R?  (only set for Z80)
	[[nodiscard]] bool prevWasLDAI() const {
		return (prev_ & (2 << 8)) != 0;
	}
	// Previous-previous instruction was a CALL?  (only set for R800)
	[[nodiscard]] bool prev2WasCall() const {
		return (prev_ & (4 << (2 * 8))) != 0;
	}
	// Previous instruction was a POP or RET?  (only set for R800)
	[[nodiscard]] bool prevWasPopRet() const {
		return (prev_ & (8 << 8)) != 0;
	}

	// Shift flags to the previous instruction positions.
	// Clear flags for current instruction.
	void endInstruction() {
		prev_ <<= 8;
	}

	// Clear all previous-flags (called on reset).
	void clearPrevious() {
		prev_ = 0;
	}

	// (for debug-only) At the start of an instruction(-block) no flags
	// should be set.
	void checkNoCurrentFlags() const {
		// Exception: we do allow a sloppy POP/RET flag, it only needs
		// to be correct after a CALL instruction.
		assert((prev_ & 0xF7) == 0);
	}


	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	z80regPair PC_;
	z80regPair AF_, BC_, DE_, HL_;
	z80regPair AF2_, BC2_, DE2_, HL2_;
	z80regPair IX_, IY_, SP_;
	bool IFF1_, IFF2_;
	byte HALT_ = 0;
	byte IM_, I_;
	byte R_, R2_; // refresh = R & Rmask | R2 & ~Rmask
	/*const*/ byte Rmask; // 0x7F for Z80, 0xFF for R800
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
	[[nodiscard]] byte getA()   const { return AF >> 8; }
	[[nodiscard]] byte getF()   const { return AF & 255; }
	[[nodiscard]] byte getB()   const { return BC >> 8; }
	[[nodiscard]] byte getC()   const { return BC & 255; }
	[[nodiscard]] byte getD()   const { return DE >> 8; }
	[[nodiscard]] byte getE()   const { return DE & 255; }
	[[nodiscard]] byte getH()   const { return HL >> 8; }
	[[nodiscard]] byte getL()   const { return HL & 255; }
	[[nodiscard]] byte getA2()  const { return AF2 >> 8; }
	[[nodiscard]] byte getF2()  const { return AF2 & 255; }
	[[nodiscard]] byte getB2()  const { return BC2 >> 8; }
	[[nodiscard]] byte getC2()  const { return BC2 & 255; }
	[[nodiscard]] byte getD2()  const { return DE2 >> 8; }
	[[nodiscard]] byte getE2()  const { return DE2 & 255; }
	[[nodiscard]] byte getH2()  const { return HL2 >> 8; }
	[[nodiscard]] byte getL2()  const { return HL2 & 255; }
	[[nodiscard]] byte getIXh() const { return IX >> 8; }
	[[nodiscard]] byte getIXl() const { return IX & 255; }
	[[nodiscard]] byte getIYh() const { return IY >> 8; }
	[[nodiscard]] byte getIYl() const { return IY & 255; }
	[[nodiscard]] byte getPCh() const { return PC >> 8; }
	[[nodiscard]] byte getPCl() const { return PC & 255; }
	[[nodiscard]] byte getSPh() const { return SP >> 8; }
	[[nodiscard]] byte getSPl() const { return SP & 255; }
	[[nodiscard]] word getAF()  const { return AF; }
	[[nodiscard]] word getBC()  const { return BC; }
	[[nodiscard]] word getDE()  const { return DE; }
	[[nodiscard]] word getHL()  const { return HL; }
	[[nodiscard]] word getAF2() const { return AF2; }
	[[nodiscard]] word getBC2() const { return BC2; }
	[[nodiscard]] word getDE2() const { return DE2; }
	[[nodiscard]] word getHL2() const { return HL2; }
	[[nodiscard]] word getIX()  const { return IX; }
	[[nodiscard]] word getIY()  const { return IY; }
	[[nodiscard]] word getPC()  const { return PC; }
	[[nodiscard]] word getSP()  const { return SP; }
	[[nodiscard]] byte getIM()  const { return IM; }
	[[nodiscard]] byte getI()   const { return I; }
	[[nodiscard]] byte getR()   const { return (R & 0x7F) | (R2 & 0x80); }
	[[nodiscard]] bool getIFF1()     const { return IFF1; }
	[[nodiscard]] bool getIFF2()     const { return IFF2; }
	[[nodiscard]] bool getHALT()     const { return HALT; }

	void setA(byte x)   { AF = (AF & 0x00FF) | (x << 8); }
	void setF(byte x)   { AF = (AF & 0xFF00) | x; }
	void setB(byte x)   { BC = (BC & 0x00FF) | (x << 8); }
	void setC(byte x)   { BC = (BC & 0xFF00) | x; }
	void setD(byte x)   { DE = (DE & 0x00FF) | (x << 8); }
	void setE(byte x)   { DE = (DE & 0xFF00) | x; }
	void setH(byte x)   { HL = (HL & 0x00FF) | (x << 8); }
	void setL(byte x)   { HL = (HL & 0xFF00) | x; }
	void setA2(byte x)  { AF2 = (AF2 & 0x00FF) | (x << 8); }
	void setF2(byte x)  { AF2 = (AF2 & 0xFF00) | x; }
	void setB2(byte x)  { BC2 = (BC2 & 0x00FF) | (x << 8); }
	void setC2(byte x)  { BC2 = (BC2 & 0xFF00) | x; }
	void setD2(byte x)  { DE2 = (DE2 & 0x00FF) | (x << 8); }
	void setE2(byte x)  { DE2 = (DE2 & 0xFF00) | x; }
	void setH2(byte x)  { HL2 = (HL2 & 0x00FF) | (x << 8); }
	void setL2(byte x)  { HL2 = (HL2 & 0xFF00) | x; }
	void setIXh(byte x) { IX = (IX & 0x00FF) | (x << 8); }
	void setIXl(byte x) { IX = (IX & 0xFF00) | x; }
	void setIYh(byte x) { IY = (IY & 0x00FF) | (x << 8); }
	void setIYl(byte x) { IY = (IY & 0xFF00) | x; }
	void setPCh(byte x) { PC = (PC & 0x00FF) | (x << 8); }
	void setPCl(byte x) { PC = (PC & 0xFF00) | x; }
	void setSPh(byte x) { SP = (SP & 0x00FF) | (x << 8); }
	void setSPl(byte x) { SP = (SP & 0xFF00) | x; }
	void setAF(word x)  { AF = x; }
	void setBC(word x)  { BC = x; }
	void setDE(word x)  { DE = x; }
	void setHL(word x)  { HL = x; }
	void setAF2(word x) { AF2 = x; }
	void setBC2(word x) { BC2 = x; }
	void setDE2(word x) { DE2 = x; }
	void setHL2(word x) { HL2 = x; }
	void setIX(word x)  { IX = x; }
	void setIY(word x)  { IY = x; }
	void setPC(word x)  { PC = x; }
	void setSP(word x)  { SP = x; }
	void setIM(byte x)  { IM = x; }
	void setI(byte x)   { I = x; }
	void setR(byte x)   { R = x; R2 = x; }
	void setIFF1(bool x)     { IFF1 = x; }
	void setIFF2(bool x)     { IFF2 = x; }
	void setHALT(bool x)     { HALT = x; }

	void incR(byte x) { R += x; }

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
