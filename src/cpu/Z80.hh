// $Id$

#ifndef Z80_HH
#define Z80_HH

#include "CPUClock.hh"
#include <cassert>

namespace openmsx {

class Z80TYPE : public CPUClock
{
protected:
	static const int CLOCK_FREQ = 3579545;
	static const int WAIT_CYCLES = 1;

	Z80TYPE(const EmuTime& time, Scheduler& scheduler)
		: CPUClock(time, scheduler)
	{
	}

	inline void M1_DELAY()       { add(1 + WAIT_CYCLES); }
	inline void ADD_16_8_DELAY() { add(5); }
	inline void OP_16_16_DELAY() { add(7); }
	inline void INC_16_DELAY()   { add(2); }
	inline void BLOCK_DELAY()    { add(5); }
	inline void RLD_DELAY()      { add(4); }
	inline void EX_SP_HL_DELAY() { add(2); }
	inline void LD_SP_HL_DELAY() { add(2); }
	inline void LDI_DELAY()      { add(2); }
	inline void DD_CB_DELAY()    { add(2); }
	inline void PARALLEL_DELAY() { add(2); }
	inline void NMI_DELAY()      { add(2); } // should take 11 cycles in total
	inline void IM0_DELAY()      { add(4); } // should take 13 cycles in total,
	inline void IM1_DELAY()      { add(4); } //   verified on real MSX (thnx to enen)
	inline void IM2_DELAY()      { add(4); } // should take 19 cycles in total
	inline void PUSH_DELAY()     { add(1); }
	inline void INC_DELAY()      { add(1); }
	inline void SMALL_DELAY()    { add(1); }
	inline void SET_IM_DELAY()   { }
	inline void DI_DELAY()       { }
	inline void RETN_DELAY()     { }
	inline void MULUB_DELAY()    { assert(false); }
	inline void MULUW_DELAY()    { assert(false); }
	inline unsigned haltStates() { return 4 + WAIT_CYCLES; } // HALT + M1
	inline bool hasMul() const   { return false; }

	inline void PRE_RDMEM_OPCODE(unsigned /*address*/) { add(1); }
	inline void PRE_RDMEM       (unsigned /*address*/) { add(1); }
	inline void PRE_WRMEM       (unsigned /*address*/) { add(1); }
	inline void POST_MEM        (unsigned /*address*/) { add(2); }

	inline void PRE_IO (unsigned /*port*/) { add(1); }
	inline void POST_IO(unsigned /*port*/) { add(3); }

	inline void R800Refresh()
	{
		// nothing
	}

	static const int
	CC_LD_A_SS  = 5+3,       CC_LD_A_SS_1  = 5+1,
	CC_LD_A_NN  = 5+3+3+3,   CC_LD_A_NN_1  = 5+1,   CC_LD_A_NN_2  = 5+3+3+1,
	CC_LD_R_N   = 5+3,       CC_LD_R_N_1   = 5+1,
	CC_LD_R_HL  = 5+3,       CC_LD_R_HL_1  = 5+1,
	CC_LD_R_XIX = 5+3+5+3,   CC_LD_R_XIX_1 = 5+1,   CC_LD_R_XIX_2 = 5+3+5+1, // +5
	CC_LD_HL_R  = 5+3,       CC_LD_HL_R_1  = 5+1,
	CC_LD_HL_N  = 5+3+3,     CC_LD_HL_N_1  = 5+1,   CC_LD_HL_N_2  = 5+3+1,
	CC_LD_SS_A  = 5+3,       CC_LD_SS_A_1  = 5+1,
	CC_LD_NN_A  = 5+3+3+3,   CC_LD_NN_A_1  = 5+1,   CC_LD_NN_A_2  = 5+3+3+1,
	CC_LD_XIX_R = 5+3+5+3,   CC_LD_XIX_R_1 = 5+1,   CC_LD_XIX_R_2 = 5+3+5+1, // +5
	CC_LD_XIX_N = 5+3+5+3,   CC_LD_XIX_N_1 = 5+1,   CC_LD_XIX_N_2 = 5+3+5+1, // +5
	CC_LD_HL_XX = 5+3+3+3+3, CC_LD_HL_XX_1 = 5+1,   CC_LD_HL_XX_2 = 5+3+3+1,
	CC_LD_SS_NN = 5+3+3,     CC_LD_SS_NN_1 = 5+1,
	CC_LD_XX_HL = 5+3+3+3+3, CC_LD_XX_HL_1 = 5+1,   CC_LD_XX_HL_2 = 5+3+3+1,

	CC_CP_N     = 5+3,       CC_CP_N_1     = 5+1,
	CC_CP_XHL   = 5+3,       CC_CP_XHL_1   = 5+1,
	CC_CP_XIX   = 5+3+5+3,   CC_CP_XIX_1   = 5+1,   CC_CP_XIX_2   = 5+3+5+1, // +5
	CC_INC_XHL  = 5+4+3,     CC_INC_XHL_1  = 5+1,   CC_INC_XHL_2  = 5+4+1,
	CC_INC_XIX  = 5+3+5+4+3, CC_INC_XIX_1  = 5+1,   EE_INC_XIX    = 8,       // +5

	CC_LDI      = 5+5+3+5,   CC_LDI_1      = 5+5+1, CC_LDI_2      = 5+5+3+1,
	CC_CPI      = 5+5+3+5,   CC_CPI_1      = 5+5+1,

	CC_PUSH     = 6+3+3,     CC_PUSH_1     = 6+1,
	CC_POP      = 5+3+3,     CC_POP_1      = 5+1,
	CC_CALL     = 5+3+4+3+3, CC_CALL_1     = 5+1,   EE_CALL       = 6,
	CC_CALL_A   = 5+3+4+3+3, CC_CALL_B     = 5+3+3,
	CC_RST      = 6+3+3,
	CC_RET      = 5+3+3,
	CC_RET_C_A  = 6+3+3,     CC_RET_C_B    = 5,     EE_RET_C      = 1,
	CC_RETN     = 5+5+3+3,   EE_RETN       = 5,
	CC_JP       = 5+3+3,     CC_JP_1       = 5+1,
	CC_JR       = 5+3+5,     CC_JR_1       = 5+1,
	CC_DJNZ     = 6+3+5,     EE_DJNZ       = 1,

	CC_EX_SP_HL = 5+3+4+3+5, CC_EX_SP_HL_1 = 5+1,   CC_EX_SP_HL_2 = 5+3+4+1,

	CC_BIT_XHL  = 5+5+4,     CC_BIT_XHL_1  = 5+5+1,
	CC_BIT_XIX  = 5+3+5+4,   CC_BIT_XIX_1  = 5+3+5+1,                        // +5
	CC_SET_XHL  = 5+5+4+3,   CC_SET_XHL_1  = 5+5+1, CC_SET_XHL_2  = 5+5+4+1,
	CC_SET_XIX  = 5+3+5+4+3, EE_SET_XIX    = 3,                              // +5

	CC_RLD      = 5+5+3+4+3, CC_RLD_1      = 5+5+1, CC_RLD_2      = 5+5+3+4+1,

	CC_IN_A_N   = 5+3+4,     CC_IN_A_N_1   = 5+1,   CC_IN_A_N_2   = 5+3+1,
	CC_IN_R_C   = 5+5+4,     CC_IN_R_C_1   = 5+5+1,
	CC_INI      = 5+6+4+3,   CC_INI_1      = 5+6+1, CC_INI_2      = 5+6+4+1,
	CC_OUT_N_A  = 5+3+4,     CC_OUT_N_A_1  = 5+1,   CC_OUT_N_A_2  = 5+3+1,
	CC_OUT_C_R  = 5+5+4,     CC_OUT_C_R_1  = 5+5+1,
	CC_OUTI     = 5+6+3+4,   CC_OUTI_1     = 5+6+1, CC_OUTI_2     = 5+6+3+1,

	CC_NMI      = 5+3+3,     EE_NMI_1      = -1,
	CC_IRQ0     = 7+3+3,     EE_IRQ0_1     = 1,
	CC_IRQ1     = 7+3+3,     EE_IRQ1_1     = 1,
	CC_IRQ2     = 7+3+3+3+3, EE_IRQ2_1     = 1,     CC_IRQ2_2     = 7+3+3+1,

	CC_MAIN     = 1,
	CC_PREFIX   = 5+1,
	CC_DD_CB    = 5+1, // +5
	EE_ED       = 5,
	CC_MEM      = 3;
};

} // namespace openmsx

#endif
