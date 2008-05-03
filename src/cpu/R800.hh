// $Id$

#ifndef R800_HH
#define R800_HH

#include "CPUClock.hh"
#include "Clock.hh"
#include "likely.hh"

namespace openmsx {

class R800TYPE : public CPUClock
{
public:
	void updateVisiblePage(byte page, byte primarySlot, byte secondarySlot)
	{
		memoryDelay[page] =
			memoryDelays[page][primarySlot][secondarySlot];
	}
	void setDRAMmode(bool dram)
	{
		// TODO currently hardcoded, move to config file?
		unsigned val = (dram ? 1 : 2) - OFFSET;
		memoryDelays[0][0][0] = val; // BIOS
		memoryDelays[1][0][0] = val; // BASIC
		memoryDelays[0][3][1] = val; // SUB-ROM
		memoryDelays[1][3][1] = val; // KANJI-DRIVER
	}

protected:
	static const int CLOCK_FREQ = 7159090;

	inline unsigned haltStates() { return 1; } // HALT + M1 // TODO check this
	inline bool hasMul() const   { return true; }

	R800TYPE(const EmuTime& time, Scheduler& scheduler)
		: CPUClock(time, scheduler)
		, lastRefreshTime(time)
		, lastPage(-1)
	{
		// TODO currently hardcoded, move to config file?
		for (int page = 0; page < 4; ++page) {
			for (int prim = 0; prim < 4; ++prim) {
				for (int sec = 0; sec < 4; ++sec) {
					unsigned val;
					if ((prim == 1) || (prim == 2)) {
						// external slot
						val = 3;
					} else if ((prim == 3) && (sec == 0)) {
						// internal RAM
						val = 1;
					} else {
						// internal ROM
						val = 2;
					}
					memoryDelays[page][prim][sec] = val - OFFSET;
				}
			}
		}
		for (int page = 0; page < 4; ++page) {
			memoryDelay[page] = memoryDelays[page][0][0];
		}
	}

	inline void PRE_RDMEM_OPCODE(unsigned address)
	{
		if (likely(memoryDelay[address >> 14] == (1 - OFFSET))) {
			int newPage = address >> 8;
			if (unlikely(newPage != lastPage)) {
				lastPage = newPage;
				add(1);
			}
		} else {
			lastPage = -1;
			add(1);
		}
	}
	inline void PRE_RDMEM(unsigned address)
	{
		if (likely(memoryDelay[address >> 14] == (1 - OFFSET))) {
			int newPage = (address >> 8) + 256;
			if (unlikely(newPage != lastPage)) {
				lastPage = newPage;
				add(1);
			}
		} else {
			lastPage = -1;
			add(1);
		}
	}
	inline void PRE_WRMEM(unsigned /*address*/)
	{
		lastPage = -1;
		add(1);
	}
	inline void POST_MEM(unsigned address)
	{
		add(memoryDelay[address >> 14] + OFFSET);
	}

	inline void PRE_IO (unsigned /*port*/) { }
	inline void POST_IO(unsigned /*port*/) {
		// TODO is this correct or does it just take 4 clock cycles
		lastPage = -1;
		add(3);
	}

	inline void R800Refresh()
	{
		// documentation says refresh every 222 clocks
		//  duration:  256/1024KB  13.5 clocks
		//             512KB       21.5 clocks
		EmuTime time = getTimeFast();
		if (unlikely(lastRefreshTime.getTicksTill_fast(time) >= 222)) {
			lastRefreshTime.advance_fast(time);
			add(22);
		}
	}

	static const unsigned
		CC_LD_A_SS   = 2, CC_LD_A_SS_1  = 1,
		CC_LD_A_NN   = 4, CC_LD_A_NN_1  = 1, CC_LD_A_NN_2  = 3,
		CC_LD_A_I    = 2,
		CC_LD_R_R    = 1,
		CC_LD_R_N    = 2, CC_LD_R_N_1   = 2,
		CC_LD_R_HL   = 2, CC_LD_R_HL_1  = 1,
		CC_LD_R_XIX  = 4, CC_LD_R_XIX_1 = 1, CC_LD_R_XIX_2 = 3, // +1
		CC_LD_HL_R   = 2, CC_LD_HL_R_1  = 1,
		CC_LD_HL_N   = 3, CC_LD_HL_N_1  = 1, CC_LD_HL_N_2  = 2,
		CC_LD_SS_A   = 2, CC_LD_SS_A_1  = 1,
		CC_LD_NN_A   = 4, CC_LD_NN_A_1  = 1, CC_LD_NN_A_2  = 3,
		CC_LD_XIX_R  = 4, CC_LD_XIX_R_1 = 1, CC_LD_XIX_R_2 = 3, // +1
		CC_LD_XIX_N  = 4, CC_LD_XIX_N_1 = 1, CC_LD_XIX_N_2 = 3, // +1
		CC_LD_HL_XX  = 5, CC_LD_HL_XX_1 = 1, CC_LD_HL_XX_2 = 3,
		CC_LD_SP_HL  = 1,
		CC_LD_SS_NN  = 3, CC_LD_SS_NN_1 = 1,
		CC_LD_XX_HL  = 5, CC_LD_XX_HL_1 = 1, CC_LD_XX_HL_2 = 3,

		CC_CP_R      = 1,
		CC_CP_N      = 2, CC_CP_N_1     = 1,
		CC_CP_XHL    = 2, CC_CP_XHL_1   = 1,
		CC_CP_XIX    = 4, CC_CP_XIX_1   = 1, CC_CP_XIX_2   = 3, // +1
		CC_INC_R     = 1,
		CC_INC_XHL   = 4, CC_INC_XHL_1  = 1, CC_INC_XHL_2  = 3,
		CC_INC_XIX   = 6, CC_INC_XIX_1  = 1, EE_INC_XIX    = 3, // +1
		CC_INC_SS    = 1,
		CC_ADD_HL_SS = 1,
		CC_ADC_HL_SS = 2,

		CC_LDI       = 4, CC_LDI_1      = 2, CC_LDI_2      = 3,
		CC_LDIR      = 4,
		CC_CPI       = 4, CC_CPI_1      = 2,
		CC_CPIR      = 5,

		CC_PUSH      = 4, CC_PUSH_1     = 2,
		CC_POP       = 3, CC_POP_1      = 1,
		CC_CALL      = 5, CC_CALL_1     = 1, EE_CALL       = 1,
		CC_CALL_A    = 5, CC_CALL_B     = 3,
		CC_RST       = 4,
		CC_RET_A     = 3, CC_RET_B      = 1, EE_RET_C      = 0,
		CC_RETN      = 5, EE_RETN       = 2,
		CC_JP        = 3, CC_JP_1       = 1,
		CC_JP_HL     = 1,
		CC_JR_A      = 3, CC_JR_B       = 2, CC_JR_1       = 1,
		CC_DJNZ      = 3, EE_DJNZ       = 0,
	
		CC_EX_SP_HL  = 5, CC_EX_SP_HL_1 = 1, CC_EX_SP_HL_2 = 3,

		CC_BIT_R     = 2,
		CC_BIT_XHL   = 3, CC_BIT_XHL_1  = 2,
		CC_BIT_XIX   = 4, CC_BIT_XIX_1  = 3,                    // +1
		CC_SET_R     = 2,
		CC_SET_XHL   = 5, CC_SET_XHL_1  = 2, CC_SET_XHL_2  = 4,
		CC_SET_XIX   = 6, EE_SET_XIX    = 2,                    // +1

		CC_RLA       = 1,
		CC_RLD       = 5, CC_RLD_1      = 2, CC_RLD_2      = 4,

		CC_IN_A_N    = 3, CC_IN_A_N_1   = 1, CC_IN_A_N_2   = 2,
		CC_IN_R_C    = 3, CC_IN_R_C_1   = 2,
		CC_INI       = 4, CC_INI_1      = 2, CC_INI_2      = 3,
		CC_INIR      = 4,
		CC_OUT_N_A   = 3, CC_OUT_N_A_1  = 1, CC_OUT_N_A_2  = 2,
		CC_OUT_C_R   = 3, CC_OUT_C_R_1  = 2,
		CC_OUTI      = 4, CC_OUTI_1     = 2, CC_OUTI_2     = 3,
		CC_OTIR      = 4,

		CC_EX        = 1,
		CC_NOP       = 1,
		CC_CCF       = 1,
		CC_SCF       = 1,
		CC_DAA       = 1,
		CC_NEG       = 2,
		CC_CPL       = 1,
		CC_DI        = 2,
		CC_EI        = 1,
		CC_HALT      = 1, // TODO check
		CC_IM        = 3,

		CC_MULUB     = 14,
		CC_MULUW     = 36,

		CC_NMI       = 4, EE_NMI_1      = 0,                    // TODO check
		CC_IRQ0      = 4, EE_IRQ0_1     = 0,                    // TODO check
		CC_IRQ1      = 4, EE_IRQ1_1     = 0,                    // TODO check
		CC_IRQ2      = 6, EE_IRQ2_1     = 0, CC_IRQ2_2     = 4, // TODO check

		CC_MAIN      = 0,
		CC_DD        = 1,
		CC_PREFIX    = 1,
		CC_DD_CB     = 1, // +1
		EE_ED        = 1,
		CC_MEM       = 1;

private:
	Clock<CLOCK_FREQ> lastRefreshTime;
	int lastPage;

	unsigned memoryDelays[4][4][4];
	unsigned memoryDelay[4];
	static const unsigned OFFSET = 1; // to allow test (x == 0) iso (x == 1)
};

} // namespace openmsx

#endif
