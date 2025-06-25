#ifndef Z80_HH
#define Z80_HH

#include "CPUClock.hh"

#include "inline.hh"

#include <cassert>

namespace openmsx {

class CPURegs;

class Z80TYPE : public CPUClock
{
protected:
	template<bool> struct Normalize { static constexpr bool value = false; };

	static constexpr int CLOCK_FREQ = 3579545;
	static constexpr int WAIT_CYCLES = 1;
	static constexpr unsigned HALT_STATES = 4 + WAIT_CYCLES; // HALT + M1
	static constexpr bool IS_R800 = false;

	Z80TYPE(EmuTime time, Scheduler& scheduler_)
		: CPUClock(time, scheduler_)
	{
	}

	template<bool, bool> ALWAYS_INLINE void PRE_MEM  (unsigned /*address*/) const {}
	template<      bool> ALWAYS_INLINE void POST_MEM (unsigned /*address*/) const {}
	template<bool, bool> ALWAYS_INLINE void PRE_WORD (unsigned /*address*/) const {}
	template<      bool> ALWAYS_INLINE void POST_WORD(unsigned /*address*/) const {}

	ALWAYS_INLINE void R800Refresh(CPURegs& /*R*/) const {}
	ALWAYS_INLINE void R800ForcePageBreak() const {}

	ALWAYS_INLINE void setMemPtr(unsigned x) { memptr = x; }
	[[nodiscard]] ALWAYS_INLINE unsigned getMemPtr() const { return memptr; }

	static constexpr int
	CC_LD_A_SS   = 5+3,       CC_LD_A_SS_1  = 5+1,
	CC_LD_A_NN   = 5+3+3+3,   CC_LD_A_NN_1  = 5+1,   CC_LD_A_NN_2  = 5+3+3+1,
	CC_LD_A_I    = 5+6,
	CC_LD_R_R    = 5,
	CC_LD_R_N    = 5+3,       CC_LD_R_N_1   = 5+1,
	CC_LD_R_HL   = 5+3,       CC_LD_R_HL_1  = 5+1,
	CC_LD_R_XIX  = 5+3+5+3,   CC_LD_R_XIX_1 = 5+1,   CC_LD_R_XIX_2 = 5+3+5+1, // +5
	CC_LD_HL_R   = 5+3,       CC_LD_HL_R_1  = 5+1,
	CC_LD_HL_N   = 5+3+3,     CC_LD_HL_N_1  = 5+1,   CC_LD_HL_N_2  = 5+3+1,
	CC_LD_SS_A   = 5+3,       CC_LD_SS_A_1  = 5+1,
	CC_LD_NN_A   = 5+3+3+3,   CC_LD_NN_A_1  = 5+1,   CC_LD_NN_A_2  = 5+3+3+1,
	CC_LD_XIX_R  = 5+3+5+3,   CC_LD_XIX_R_1 = 5+1,   CC_LD_XIX_R_2 = 5+3+5+1, // +5
	CC_LD_XIX_N  = 5+3+5+3,   CC_LD_XIX_N_1 = 5+1,   CC_LD_XIX_N_2 = 5+3+5+1, // +5
	CC_LD_HL_XX  = 5+3+3+3+3, CC_LD_HL_XX_1 = 5+1,   CC_LD_HL_XX_2 = 5+3+3+1,
	CC_LD_SP_HL  = 7,
	CC_LD_SS_NN  = 5+3+3,     CC_LD_SS_NN_1 = 5+1,
	CC_LD_XX_HL  = 5+3+3+3+3, CC_LD_XX_HL_1 = 5+1,   CC_LD_XX_HL_2 = 5+3+3+1,

	CC_CP_R      = 5,
	CC_CP_N      = 5+3,       CC_CP_N_1     = 5+1,
	CC_CP_XHL    = 5+3,       CC_CP_XHL_1   = 5+1,
	CC_CP_XIX    = 5+3+5+3,   CC_CP_XIX_1   = 5+1,   CC_CP_XIX_2   = 5+3+5+1, // +5
	CC_INC_R     = 5,
	CC_INC_XHL   = 5+4+3,     CC_INC_XHL_1  = 5+1,   CC_INC_XHL_2  = 5+4+1,
	CC_INC_XIX   = 5+3+5+4+3, CC_INC_XIX_1  = 5+1,   EE_INC_XIX    = 8,       // +5
	CC_INC_SS    = 7,
	CC_ADD_HL_SS = 5+4+3,
	CC_ADC_HL_SS = 5+5+4+3,

	CC_LDI       = 5+5+3+5,   CC_LDI_1      = 5+5+1, CC_LDI_2      = 5+5+3+1,
	CC_LDIR      = 5+5+3+5+5,
	CC_CPI       = 5+5+3+5,   CC_CPI_1      = 5+5+1,
	CC_CPIR      = 5+5+3+5+5,

	CC_PUSH      = 6+3+3,     CC_PUSH_1     = 6+1,
	CC_POP       = 5+3+3,     CC_POP_1      = 5+1,
	CC_CALL      = 5+3+4+3+3, CC_CALL_1     = 5+1,   EE_CALL       = 6,
	CC_CALL_A    = 5+3+4+3+3, CC_CALL_B     = 5+3+3,
	CC_RST       = 6+3+3,
	CC_RET_A     = 5+3+3,     CC_RET_B      = 5,     EE_RET_C      = 1,
	CC_RETN      = 5+5+3+3,   EE_RETN       = 5,
	CC_JP_A      = 5+3+3,     CC_JP_B       = 5+3+3, CC_JP_1       = 5+1,
	CC_JP_HL     = 5,
	CC_JR_A      = 5+3+5,     CC_JR_B       = 5+3,   CC_JR_1       = 5+1,
	CC_DJNZ      = 6+3+5,     EE_DJNZ       = 1,

	CC_EX_SP_HL  = 5+3+4+3+5, CC_EX_SP_HL_1 = 5+1,   CC_EX_SP_HL_2 = 5+3+4+1,

	CC_BIT_R     = 5+5,
	CC_BIT_XHL   = 5+5+4,     CC_BIT_XHL_1  = 5+5+1,
	CC_BIT_XIX   = 5+3+5+4,   CC_BIT_XIX_1  = 5+3+5+1,                        // +5
	CC_SET_R     = 5+5,
	CC_SET_XHL   = 5+5+4+3,   CC_SET_XHL_1  = 5+5+1, CC_SET_XHL_2  = 5+5+4+1,
	CC_SET_XIX   = 5+3+5+4+3, EE_SET_XIX    = 3,                              // +5

	CC_RLA       = 5,
	CC_RLD       = 5+5+3+4+3, CC_RLD_1      = 5+5+1, CC_RLD_2      = 5+5+3+4+1,

	CC_IN_A_N    = 5+3+4,     CC_IN_A_N_1   = 5+1,   CC_IN_A_N_2   = 5+3+1,
	CC_IN_R_C    = 5+5+4,     CC_IN_R_C_1   = 5+5+1,
	CC_INI       = 5+6+4+3,   CC_INI_1      = 5+6+1, CC_INI_2      = 5+6+4+1,
	CC_INIR      = 5+6+4+3+5,
	CC_OUT_N_A   = 5+3+4,     CC_OUT_N_A_1  = 5+1,   CC_OUT_N_A_2  = 5+3+1,
	CC_OUT_C_R   = 5+5+4,     CC_OUT_C_R_1  = 5+5+1,
	CC_OUTI      = 5+6+3+4,   CC_OUTI_1     = 5+6+1, CC_OUTI_2     = 5+6+3+1,
	CC_OTIR      = 5+6+3+4+5,

	CC_EX        = 5,
	CC_NOP       = 5,
	CC_CCF       = 5,
	CC_SCF       = 5,
	CC_DAA       = 5,
	CC_NEG       = 5+5,
	CC_CPL       = 5,
	CC_DI        = 5,
	CC_EI        = 5,
	CC_HALT      = 5,
	CC_IM        = 5+5,

	CC_MULUB     = 0,
	CC_MULUW     = 0,

	CC_NMI       = 5+3+3,     EE_NMI_1      = -1,
	CC_IRQ0      = 7+3+3,     EE_IRQ0_1     = 1,
	CC_IRQ1      = 7+3+3,     EE_IRQ1_1     = 1,
	CC_IRQ2      = 7+3+3+3+3, EE_IRQ2_1     = 1,     CC_IRQ2_2     = 7+3+3+1,

	CC_MAIN      = 1,
	CC_DD        = 5,
	CC_PREFIX    = 5+1,
	CC_DD_CB     = 5+1, // +5
	EE_ED        = 5,
	CC_RDMEM     = 3,
	CC_WRMEM     = 3;

	// versions (version number shared with CPUCore<> class)
	//  1 -> initial version
	//  2 -> moved memptr from CPUCore to here
	template<typename Archive>
	void serialize(Archive& ar, unsigned version)
	{
		CPUClock::serialize(ar, version);
		if (version >= 2) {
			ar.serialize("memptr", memptr);
		}
	}

private:
	unsigned memptr;
};


} // namespace openmsx

#endif
