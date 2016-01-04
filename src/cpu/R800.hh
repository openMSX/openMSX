#ifndef R800_HH
#define R800_HH

#include "CPUClock.hh"
#include "CPURegs.hh"
#include "Clock.hh"
#include "likely.hh"
#include "inline.hh"

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
	template<bool B> struct Normalize { static const bool value = B; };

	static const int CLOCK_FREQ = 7159090;

	ALWAYS_INLINE unsigned haltStates() const { return 1; } // TODO check this
	ALWAYS_INLINE bool isR800() const { return true; }

	R800TYPE(EmuTime::param time, Scheduler& scheduler_)
		: CPUClock(time, scheduler_)
		, lastRefreshTime(time)
	{
		R800ForcePageBreak();

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

	ALWAYS_INLINE void R800ForcePageBreak()
	{
		lastPage = -1;
	}

	template <bool PRE_PB, bool POST_PB>
	ALWAYS_INLINE void PRE_MEM(unsigned address)
	{
		int newPage = address >> 8;
		if (PRE_PB) {
			// there is a statically predictable page break at this
			// point -> 'add(1)' moved to static cost table
		} else {
			if (unlikely(newPage != lastPage) ||
			    unlikely(extraMemoryDelay[address >> 14])) {
				add(1);
			}
		}
		if (!POST_PB) {
			lastPage = newPage;
		}
	}
	template <bool POST_PB>
	ALWAYS_INLINE void POST_MEM(unsigned address)
	{
		add(extraMemoryDelay[address >> 14]);
		if (POST_PB) {
			R800ForcePageBreak();
		}
	}
	template <bool PRE_PB, bool POST_PB>
	ALWAYS_INLINE void PRE_WORD(unsigned address)
	{
		int newPage = address >> 8;
		if (PRE_PB) {
			// there is a statically predictable page break at this
			// point -> 'add(1)' moved to static cost table
			if (unlikely(extraMemoryDelay[address >> 14])) {
				add(1);
			}
		} else {
			if (unlikely(extraMemoryDelay[address >> 14])) {
				add(2);
			} else if (unlikely(newPage != lastPage)) {
				add(1);
			}
		}
		if (!POST_PB) {
			lastPage = newPage;
		}
	}
	template <bool POST_PB>
	ALWAYS_INLINE void POST_WORD(unsigned address)
	{
		add(2 * extraMemoryDelay[address >> 14]);
		if (POST_PB) {
			R800ForcePageBreak();
		}
	}

	ALWAYS_INLINE void R800Refresh(CPURegs& R)
	{
		// atoc documentation says refresh every 222 clocks
		//  duration:  256/1024KB  13.5 clocks
		//             512KB       21.5 clocks
		// But 26/210 matches measurements much better
		//   (loosly based on old measurements by Jon on his analogue scope)
		EmuTime time = getTimeFast();
		if (unlikely(lastRefreshTime.getTicksTill_fast(time) >= 210)) {
			R800RefreshSlow(time, R); // slow-path not inline
		}
	}
	NEVER_INLINE void R800RefreshSlow(EmuTime::param time, CPURegs& R)
	{
		do {
			lastRefreshTime += 210;
		} while (unlikely(lastRefreshTime.getTicksTill_fast(time) >= 210));
		waitForEvenCycle(0);
		add(25);
		R800ForcePageBreak(); // TODO check this
		R.incR(1);
	}

	void setTime(EmuTime::param time)
	{
		// Base class implementation.
		CPUClock::setTime(time);

		// Otherwise advance_fast() in R800Refresh() above, gets a too
		// large time interval.
		lastRefreshTime.reset(time);
	}

	ALWAYS_INLINE void setMemPtr(unsigned) { /* nothing*/ }
	ALWAYS_INLINE unsigned getMemPtr() const { return 0; } // dummy value

	static const int I  = 6; // cycles for an I/O operation
	static const int O  = 1; // wait for one cycle and wait for next even
	                         // clock cycle (to sync with slower IO bus)
	                         // the latter part must be implemented
	                         // dynamically (not here in static tables)
	static const int P  = 1; // cycles for a (statically known) page-break

	static const int
		CC_LD_A_SS   = 1+P+1, CC_LD_A_SS_1  = 1+P,
		CC_LD_A_NN   = 3+P+1, CC_LD_A_NN_1  = 1, CC_LD_A_NN_2  = 3+P,
		CC_LD_A_I    = 2,
		CC_LD_R_R    = 1,
		CC_LD_R_N    = 2,     CC_LD_R_N_1   = 1,
		CC_LD_R_HL   = 1+P+1, CC_LD_R_HL_1  = 1+P,
		CC_LD_R_XIX  = 3+P+1, CC_LD_R_XIX_1 = 1, CC_LD_R_XIX_2 = 3+P, // +1
		CC_LD_HL_R   = 1+P+1, CC_LD_HL_R_1  = 1+P,
		CC_LD_HL_N   = 2+P+1, CC_LD_HL_N_1  = 1, CC_LD_HL_N_2  = 2+P,
		CC_LD_SS_A   = 1+P+1, CC_LD_SS_A_1  = 1+P,
		CC_LD_NN_A   = 3+P+1, CC_LD_NN_A_1  = 1, CC_LD_NN_A_2  = 3+P,
		CC_LD_XIX_R  = 3+P+1, CC_LD_XIX_R_1 = 1, CC_LD_XIX_R_2 = 3+P, // +1
		CC_LD_XIX_N  = 3+P+1, CC_LD_XIX_N_1 = 1, CC_LD_XIX_N_2 = 3+P, // +1
		CC_LD_HL_XX  = 3+P+2, CC_LD_HL_XX_1 = 1, CC_LD_HL_XX_2 = 3+P,
		CC_LD_SP_HL  = 1,
		CC_LD_SS_NN  = 3,     CC_LD_SS_NN_1 = 1,
		CC_LD_XX_HL  = 3+P+2, CC_LD_XX_HL_1 = 1, CC_LD_XX_HL_2 = 3+P,

		CC_CP_R      = 1,
		CC_CP_N      = 2,     CC_CP_N_1     = 1,
		CC_CP_XHL    = 1+P+1, CC_CP_XHL_1   = 1+P,
		CC_CP_XIX    = 3+P+1, CC_CP_XIX_1   = 1, CC_CP_XIX_2   = 3+P, // +1
		CC_INC_R     = 1,
		CC_INC_XHL   = 1+P+2+P+1, CC_INC_XHL_1 = 1, CC_INC_XHL_2 = 1+P+2+P,
		CC_INC_XIX   = 3+P+2+P+1, CC_INC_XIX_1 = 1, EE_INC_XIX   = 2, // +1
		CC_INC_SS    = 1,
		CC_ADD_HL_SS = 1,
		CC_ADC_HL_SS = 2,

		CC_LDI       = 2+P+1+P+1, CC_LDI_1  = 2+P, CC_LDI_2    = 2+P+1+P,
		CC_LDIR      = 2+P+1+P+1,
		CC_CPI       = 2+P+2, CC_CPI_1      = 2+P,
		CC_CPIR      = 2+P+3, // TODO check

		CC_PUSH      = 2+P+2, CC_PUSH_1     = 2+P,
		CC_POP       = 1+P+2, CC_POP_1      = 1+P,
		CC_CALL      = 3+P+2, CC_CALL_1     = 1, EE_CALL       = 1,
		CC_CALL_A    = 3+P+2, CC_CALL_B     = 3,
		CC_RST       = 2+P+2, // TODO check
		CC_RET_A     = 1+P+2, CC_RET_B      = 1, EE_RET_C      = 0,
		CC_RETN      = 2+P+2, EE_RETN       = 1, // TODO check
		CC_JP_A      = 4,     CC_JP_B       = 3, CC_JP_1       = 1,
		CC_JP_HL     = 2,
		CC_JR_A      = 3,     CC_JR_B       = 2, CC_JR_1       = 1,
		CC_DJNZ      = 3,     EE_DJNZ       = 0,

		CC_EX_SP_HL  = 1+P+4, CC_EX_SP_HL_1 = 1+P, CC_EX_SP_HL_2 = 1+P+2,

		CC_BIT_R     = 2,
		CC_BIT_XHL   = 2+P+1, CC_BIT_XHL_1  = 2+P,
		CC_BIT_XIX   = 3+P+1, CC_BIT_XIX_1  = 3+P, // +1
		CC_SET_R     = 2,
		CC_SET_XHL   = 2+P+2+P+1, CC_SET_XHL_1 = 2+P, CC_SET_XHL_2 = 2+P+2+P,
		CC_SET_XIX   = 3+P+2+P+1, EE_SET_XIX   = 1, // +1

		CC_RLA       = 1,
		CC_RLD       = 2+P+2+P+1, CC_RLD_1  = 2+P, CC_RLD_2    = 2+P+2+P,

		CC_IN_A_N    = 2+O+I,     CC_IN_A_N_1 = 1,    CC_IN_A_N_2  = 2+O,
		CC_IN_R_C    = 2+O+I,     CC_IN_R_C_1 = 2+O,
		CC_INI       = 2+O+I+P+1, CC_INI_1    = 2+O,  CC_INI_2     = 2+O+I+P,
		CC_INIR      = 2+O+I+P+1, // TODO check
		CC_OUT_N_A   = 2+O+I,     CC_OUT_N_A_1 = 1,   CC_OUT_N_A_2 = 2+O,
		CC_OUT_C_R   = 2+O+I,     CC_OUT_C_R_1 = 2+O,
		CC_OUTI      = 2+P+1+O+I, CC_OUTI_1    = 2+P, CC_OUTI_2    = 2+P+1+O,
		CC_OTIR      = 2+P+1+O+I, // TODO check

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

		CC_NMI       = 1+P+2,   EE_NMI_1      = -1,                    // TODO check
		CC_IRQ0      = 1+P+2,   EE_IRQ0_1     = -1,                    // TODO check
		CC_IRQ1      = 1+P+2,   EE_IRQ1_1     = -1,                    // TODO check
		CC_IRQ2      = 1+P+2+P+2, EE_IRQ2_1   = -1, CC_IRQ2_2 = 1+P+2+P, // TODO check

		CC_MAIN      = 0,
		CC_DD        = 1,
		CC_PREFIX    = 1,
		CC_DD_CB     = 1, // +1
		EE_ED        = 1,
		CC_RDMEM     = 1,
		CC_WRMEM     = 2;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version)
	{
		CPUClock::serialize(ar, version);
		ar.serialize("lastRefreshTime", lastRefreshTime);
		ar.serialize("lastPage", lastPage);
		ar.serialize("extraMemoryDelay", extraMemoryDelay);

		// don't serialize 'extraMemoryDelays', is initialized in
		// constructor and setDRAMmode()
	}

private:
	Clock<CLOCK_FREQ> lastRefreshTime;
	int lastPage;

	unsigned extraMemoryDelays[4][4][4];
	unsigned extraMemoryDelay[4];
};

} // namespace openmsx

#endif
