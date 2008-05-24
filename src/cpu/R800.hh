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
		extraMemoryDelay[page] =
			extraMemoryDelays[page][primarySlot][secondarySlot];
	}
	void setDRAMmode(bool dram)
	{
		// TODO currently hardcoded, move to config file?
		unsigned val = dram ? 0 : 1;
		extraMemoryDelays[0][0][0] = val; // BIOS
		extraMemoryDelays[1][0][0] = val; // BASIC
		extraMemoryDelays[0][3][1] = val; // SUB-ROM
		extraMemoryDelays[1][3][1] = val; // KANJI-DRIVER
	}

protected:
	static const int I  = 8; // cycles for an I/O operation
	static const int PW = 1; // cycles for a write operation (pre)
	static const int W  = 2; // cycles for a write operation (total)

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
						val = 2;
					} else if ((prim == 3) && (sec == 0)) {
						// internal RAM
						val = 0;
					} else {
						// internal ROM
						val = 1;
					}
					extraMemoryDelays[page][prim][sec] = val;
				}
			}
		}
		for (int page = 0; page < 4; ++page) {
			extraMemoryDelay[page] = extraMemoryDelays[page][0][0];
		}
	}

	inline void PRE_RDMEM_OPCODE(unsigned address)
	{
		int newPage = address >> 8;
		if (unlikely(newPage != lastPage) ||
		    unlikely(extraMemoryDelay[address >> 14])) {
			// page break, either because high address byte really
			// changed or because the region doesn't support the
			// CAS/RAS optimization
			add(1);
		}
		lastPage = newPage;
	}
	// TODO Not correct for 'ex (sp),hl' instruction.
	// TODO Can be optimized: in most of the cases we know when there
	//      will be a page break (because of switching between opcode
	//      fetching, data read, data write).
	inline void PRE_RDMEM(unsigned address)
	{
		int newPage = (address >> 8) + 256;
		if (unlikely(newPage != lastPage) ||
		    unlikely(extraMemoryDelay[address >> 14])) {
			add(1);
		}
		lastPage = newPage;
	}
	inline void PRE_WRMEM(unsigned address)
	{
		int newPage = (address >> 8) + 512;
		if (unlikely(newPage != lastPage) ||
		    unlikely(extraMemoryDelay[address >> 14])) {
			add(1);
		}
		lastPage = newPage;
	}
	inline void POST_MEM(unsigned address)
	{
		add(extraMemoryDelay[address >> 14]);
	}

	inline void PRE_IO (unsigned /*port*/) { }
	inline void POST_IO(unsigned /*port*/) {
		// no page-break after IO operation
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

	inline void R800ForcePageBreak()
	{
		lastPage = -1;
	}

	static const unsigned
		CC_LD_A_SS   = 2,     CC_LD_A_SS_1  = 1,
		CC_LD_A_NN   = 4,     CC_LD_A_NN_1  = 1, CC_LD_A_NN_2  = 3,
		CC_LD_A_I    = 2,
		CC_LD_R_R    = 1,
		CC_LD_R_N    = 2,     CC_LD_R_N_1   = 1,
		CC_LD_R_HL   = 2,     CC_LD_R_HL_1  = 1,
		CC_LD_R_XIX  = 4,     CC_LD_R_XIX_1 = 1, CC_LD_R_XIX_2 = 3, // +1
		CC_LD_HL_R   = 1+W,   CC_LD_HL_R_1  = 1+PW,
		CC_LD_HL_N   = 2+W,   CC_LD_HL_N_1  = 1, CC_LD_HL_N_2  = 2+PW,
		CC_LD_SS_A   = 1+W,   CC_LD_SS_A_1  = 1+PW,
		CC_LD_NN_A   = 3+W,   CC_LD_NN_A_1  = 1, CC_LD_NN_A_2  = 3+PW,
		CC_LD_XIX_R  = 3+W,   CC_LD_XIX_R_1 = 1, CC_LD_XIX_R_2 = 3+PW, // +1
		CC_LD_XIX_N  = 3+W,   CC_LD_XIX_N_1 = 1, CC_LD_XIX_N_2 = 3+PW, // +1
		CC_LD_HL_XX  = 5,     CC_LD_HL_XX_1 = 1, CC_LD_HL_XX_2 = 3,
		CC_LD_SP_HL  = 1,
		CC_LD_SS_NN  = 3,     CC_LD_SS_NN_1 = 1,
		CC_LD_XX_HL  = 3+W+W, CC_LD_XX_HL_1 = 1, CC_LD_XX_HL_2 = 3+PW,

		CC_CP_R      = 1,
		CC_CP_N      = 2,     CC_CP_N_1     = 1,
		CC_CP_XHL    = 2,     CC_CP_XHL_1   = 1,
		CC_CP_XIX    = 4,     CC_CP_XIX_1   = 1, CC_CP_XIX_2   = 3, // +1
		CC_INC_R     = 1,
		CC_INC_XHL   = 3+W,   CC_INC_XHL_1  = 1, CC_INC_XHL_2  = 3+PW,
		CC_INC_XIX   = 5+W,   CC_INC_XIX_1  = 1, EE_INC_XIX    = 2, // +1
		CC_INC_SS    = 1,
		CC_ADD_HL_SS = 1,
		CC_ADC_HL_SS = 2,

		CC_LDI       = 3+W,   CC_LDI_1      = 2, CC_LDI_2      = 3+PW,
		CC_LDIR      = 3+W+1,
		CC_CPI       = 4,     CC_CPI_1      = 2,
		CC_CPIR      = 5,

		CC_PUSH      = 2+W+W, CC_PUSH_1     = 2+PW,
		CC_POP       = 3,     CC_POP_1      = 1,
		CC_CALL      = 4+W+W, CC_CALL_1     = 1, EE_CALL       = 2,
		CC_CALL_A    = 4+W+W, CC_CALL_B     = 3,
		CC_RST       = 2+W+W,
		CC_RET_A     = 3,     CC_RET_B      = 1, EE_RET_C      = 0,
		CC_RETN      = 5,     EE_RETN       = 2,
		CC_JP        = 4,     CC_JP_1       = 1,
		CC_JP_HL     = 2,
		CC_JR_A      = 3,     CC_JR_B       = 2, CC_JR_1       = 1,
		CC_DJNZ      = 3,     EE_DJNZ       = 0,

		CC_EX_SP_HL  = 3+W+W, CC_EX_SP_HL_1 = 1, CC_EX_SP_HL_2 = 3+PW,

		CC_BIT_R     = 2,
		CC_BIT_XHL   = 3,     CC_BIT_XHL_1  = 2,
		CC_BIT_XIX   = 4,     CC_BIT_XIX_1  = 3,                    // +1
		CC_SET_R     = 2,
		CC_SET_XHL   = 4+W,   CC_SET_XHL_1  = 2, CC_SET_XHL_2  = 4+PW,
		CC_SET_XIX   = 5+W,   EE_SET_XIX    = 1,                    // +1

		CC_RLA       = 1,
		CC_RLD       = 4+W,   CC_RLD_1      = 2, CC_RLD_2      = 4+PW,

		CC_IN_A_N    = 2+I,   CC_IN_A_N_1   = 1, CC_IN_A_N_2   = 2,
		CC_IN_R_C    = 2+I,   CC_IN_R_C_1   = 2,
		CC_INI       = 2+I+W, CC_INI_1      = 2, CC_INI_2      = 2+I+PW,
		CC_INIR      = 2+I+W+1,
		CC_OUT_N_A   = 2+I,   CC_OUT_N_A_1  = 1, CC_OUT_N_A_2  = 2,
		CC_OUT_C_R   = 2+I,   CC_OUT_C_R_1  = 2,
		CC_OUTI      = 3+I,   CC_OUTI_1     = 2, CC_OUTI_2     = 3,
		CC_OTIR      = 3+I+1,

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

		CC_NMI       = 1+W+W,   EE_NMI_1      = -1,                    // TODO check
		CC_IRQ0      = 1+W+W,   EE_IRQ0_1     = -1,                    // TODO check
		CC_IRQ1      = 1+W+W,   EE_IRQ1_1     = -1,                    // TODO check
		CC_IRQ2      = 1+W+W+2, EE_IRQ2_1     = -1, CC_IRQ2_2     = 4, // TODO check

		CC_MAIN      = 0,
		CC_DD        = 1,
		CC_PREFIX    = 1,
		CC_DD_CB     = 1, // +1
		EE_ED        = 1,
		CC_RDMEM     = 1,
		CC_WRMEM     = 2;

private:
	Clock<CLOCK_FREQ> lastRefreshTime;
	int lastPage;

	unsigned extraMemoryDelays[4][4][4];
	unsigned extraMemoryDelay[4];
};

} // namespace openmsx

#endif
