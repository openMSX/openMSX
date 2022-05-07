#ifndef CPUCORE_HH
#define CPUCORE_HH

#include "CPURegs.hh"
#include "CacheLine.hh"
#include "Probe.hh"
#include "EmuTime.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include <atomic>
#include <span>
#include <string>

namespace openmsx {

class MSXCPUInterface;
class Scheduler;
class MSXMotherBoard;
class TclCallback;
class TclObject;
class Interpreter;
enum Reg8  : int;
enum Reg16 : int;

class CPUBase {}; // only for bw-compat savestates

struct II { // InstructionInfo
	// Number of instruction byte fetches since the last M1 cycle.
	// In other words, at the end of an instruction the PC register should
	// be incremented by this amount.
	int length;

	// Total duration of the instruction. At the end of the instruction
	// this value is added to the total cycle counter. For efficiency
	// reasons we only want to adjust this total once per instruction
	// instead of small incremental updates after each micro-code.
	int cycles;
};

enum class ExecIRQ {
	NMI,  // about to execute NMI routine
	IRQ,  // about to execute normal IRQ routine
	NONE, // about to execute regular instruction
};

struct CacheLines {
	const byte** read;
	      byte** write;
};

template<typename CPU_POLICY>
class CPUCore final : public CPUBase, public CPURegs, public CPU_POLICY
{
public:
	CPUCore(MSXMotherBoard& motherboard, const std::string& name,
	        const BooleanSetting& traceSetting,
	        TclCallback& diHaltCallback, EmuTime::param time);

	void setInterface(MSXCPUInterface* interf) { interface = interf; }

	/**
	 * Reset the CPU.
	 */
	void doReset(EmuTime::param time);

	void execute(bool fastForward);

	/** Request to exit the main CPU emulation loop.
	  * This method may only be called from the main thread. The CPU loop
	  * will immediately be exited (current instruction will be finished,
	  * but no new instruction will be executed).
	  */
	void exitCPULoopSync();

	/** Similar to exitCPULoopSync(), but this method may be called from
	  * any thread. Although now the loop will only be exited 'soon'.
	  */
	void exitCPULoopAsync();

	void warp(EmuTime::param time);
	[[nodiscard]] EmuTime::param getCurrentTime() const;
	void wait(EmuTime::param time);
	EmuTime waitCycles(EmuTime::param time, unsigned cycles);
	void setNextSyncPoint(EmuTime::param time);
	[[nodiscard]] CacheLines getCacheLines() {
		return {readCacheLine, writeCacheLine};
	}
	[[nodiscard]] bool isM1Cycle(unsigned address) const;

	void disasmCommand(Interpreter& interp,
	                   std::span<const TclObject> tokens,
	                   TclObject& result) const;

	/**
	 * Raises the maskable interrupt count.
	 * Devices should call MSXCPU::raiseIRQ instead, or use the IRQHelper class.
	 */
	void raiseIRQ();

	/**
	 * Lowers the maskable interrupt count.
	 * Devices should call MSXCPU::lowerIRQ instead, or use the IRQHelper class.
	 */
	void lowerIRQ();

	/**
	 * Raises the non-maskable interrupt count.
	 * Devices should call MSXCPU::raiseNMI instead, or use the IRQHelper class.
	 */
	void raiseNMI();

	/**
	 * Lowers the non-maskable interrupt count.
	 * Devices should call MSXCPU::lowerNMI instead, or use the IRQHelper class.
	 */
	void lowerNMI();

	/**
	 * Change the clock freq.
	 */
	void setFreq(unsigned freq);

	void serialize(Archive auto& ar, unsigned version);

private:
	void execute2(bool fastForward);
	[[nodiscard]] bool needExitCPULoop();
	void setSlowInstructions();
	void doSetFreq();

	// Observer<Setting>  !! non-virtual !!
	void update(const Setting& setting) noexcept;

private:
	// memory cache
	const byte* readCacheLine[CacheLine::NUM];
	byte* writeCacheLine[CacheLine::NUM];

	MSXMotherBoard& motherboard;
	Scheduler& scheduler;
	MSXCPUInterface* interface;

	const BooleanSetting& traceSetting;
	TclCallback& diHaltCallback;

	Probe<int> IRQStatus;
	Probe<void> IRQAccept;

	// dynamic freq
	BooleanSetting freqLocked;
	IntegerSetting freqValue;
	unsigned freq;

	// state machine variables
	int slowInstructions;
	int NMIStatus;

	/**
	 * Set to true when there was a rising edge on the NMI line
	 * (rising = non-active -> active).
	 * Set to false when the CPU jumps to the NMI handler address.
	 */
	bool nmiEdge;

	std::atomic<bool> exitLoop;

	/** In sync with traceSetting.getBoolean(). */
	bool tracingEnabled;

	/** An NMOS Z80 and a CMOS Z80 behave slightly differently */
	const bool isCMOS;

private:
	inline void cpuTracePre();
	inline void cpuTracePost();
	void cpuTracePost_slow();

	inline byte READ_PORT(unsigned port, unsigned cc);
	inline void WRITE_PORT(unsigned port, byte value, unsigned cc);

	template<bool PRE_PB, bool POST_PB>
	byte RDMEMslow(unsigned address, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline byte RDMEM_impl2(unsigned address, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline byte RDMEM_impl (unsigned address, unsigned cc);
	template<unsigned PC_OFFSET>
	inline byte RDMEM_OPCODE(unsigned cc);
	inline byte RDMEM(unsigned address, unsigned cc);

	template<bool PRE_PB, bool POST_PB>
	unsigned RD_WORD_slow(unsigned address, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline unsigned RD_WORD_impl2(unsigned address, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline unsigned RD_WORD_impl (unsigned address, unsigned cc);
	template<unsigned PC_OFFSET>
	inline unsigned RD_WORD_PC(unsigned cc);
	inline unsigned RD_WORD(unsigned address, unsigned cc);

	template<bool PRE_PB, bool POST_PB>
	void WRMEMslow(unsigned address, byte value, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline void WRMEM_impl2(unsigned address, byte value, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline void WRMEM_impl (unsigned address, byte value, unsigned cc);
	inline void WRMEM(unsigned address, byte value, unsigned cc);

	void WR_WORD_slow(unsigned address, unsigned value, unsigned cc);
	inline void WR_WORD(unsigned address, unsigned value, unsigned cc);

	template<bool PRE_PB, bool POST_PB>
	void WR_WORD_rev_slow(unsigned address, unsigned value, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline void WR_WORD_rev2(unsigned address, unsigned value, unsigned cc);
	template<bool PRE_PB, bool POST_PB>
	inline void WR_WORD_rev (unsigned address, unsigned value, unsigned cc);

	void executeInstructions();
	inline void nmi();
	inline void irq0();
	inline void irq1();
	inline void irq2();
	[[nodiscard]] ExecIRQ getExecIRQ() const;
	void executeSlow(ExecIRQ execIRQ);

	template<Reg8>  [[nodiscard]] inline byte     get8()  const;
	template<Reg16> [[nodiscard]] inline unsigned get16() const;
	template<Reg8>  inline void set8 (byte     x);
	template<Reg16> inline void set16(unsigned x);

	template<Reg8 DST, Reg8 SRC, int EE> inline II ld_R_R();
	template<Reg16 REG, int EE> inline II ld_sp_SS();
	template<Reg16 REG> inline II ld_SS_a();
	template<Reg8 SRC> inline II ld_xhl_R();
	template<Reg16 IXY, Reg8 SRC> inline II ld_xix_R();

	inline II ld_xhl_byte();
	template<Reg16 IXY> inline II ld_xix_byte();

	template<int EE> inline II WR_NN_Y(unsigned reg);
	template<Reg16 REG, int EE> inline II ld_xword_SS();
	template<Reg16 REG> inline II ld_xword_SS_ED();
	template<Reg16 REG> inline II ld_a_SS();

	inline II ld_xbyte_a();
	inline II ld_a_xbyte();

	template<Reg8 DST, int EE> inline II ld_R_byte();
	template<Reg8 DST> inline II ld_R_xhl();
	template<Reg8 DST, Reg16 IXY> inline II ld_R_xix();

	template<int EE> inline unsigned RD_P_XX();
	template<Reg16 REG, int EE> inline II ld_SS_xword();
	template<Reg16 REG> inline II ld_SS_xword_ED();

	template<Reg16 REG, int EE> inline II ld_SS_word();

	inline void ADC(byte reg);
	inline II adc_a_a();
	template<Reg8 SRC, int EE> inline II adc_a_R();
	inline II adc_a_byte();
	inline II adc_a_xhl();
	template<Reg16 IXY> inline II adc_a_xix();

	inline void ADD(byte reg);
	inline II add_a_a();
	template<Reg8 SRC, int EE> inline II add_a_R();
	inline II add_a_byte();
	inline II add_a_xhl();
	template<Reg16 IXY> inline II add_a_xix();

	inline void AND(byte reg);
	inline II and_a();
	template<Reg8 SRC, int EE> inline II and_R();
	inline II and_byte();
	inline II and_xhl();
	template<Reg16 IXY> inline II and_xix();

	inline void CP(byte reg);
	inline II cp_a();
	template<Reg8 SRC, int EE> inline II cp_R();
	inline II cp_byte();
	inline II cp_xhl();
	template<Reg16 IXY> inline II cp_xix();

	inline void OR(byte reg);
	inline II or_a();
	template<Reg8 SRC, int EE> inline II or_R();
	inline II or_byte();
	inline II or_xhl();
	template<Reg16 IXY> inline II or_xix();

	inline void SBC(byte reg);
	inline II sbc_a_a();
	template<Reg8 SRC, int EE> inline II sbc_a_R();
	inline II sbc_a_byte();
	inline II sbc_a_xhl();
	template<Reg16 IXY> inline II sbc_a_xix();

	inline void SUB(byte reg);
	inline II sub_a();
	template<Reg8 SRC, int EE> inline II sub_R();
	inline II sub_byte();
	inline II sub_xhl();
	template<Reg16 IXY> inline II sub_xix();

	inline void XOR(byte reg);
	inline II xor_a();
	template<Reg8 SRC, int EE> inline II xor_R();
	inline II xor_byte();
	inline II xor_xhl();
	template<Reg16 IXY> inline II xor_xix();

	inline byte DEC(byte reg);
	template<Reg8 REG, int EE> inline II dec_R();
	template<int EE> inline void DEC_X(unsigned x);
	inline II dec_xhl();
	template<Reg16 IXY> inline II dec_xix();

	inline byte INC(byte reg);
	template<Reg8 REG, int EE> inline II inc_R();
	template<int EE> inline void INC_X(unsigned x);
	inline II inc_xhl();
	template<Reg16 IXY> inline II inc_xix();

	template<Reg16 REG> inline II adc_hl_SS();
	inline II adc_hl_hl();
	template<Reg16 REG1, Reg16 REG2, int EE> inline II add_SS_TT();
	template<Reg16 REG, int EE> inline II add_SS_SS();
	template<Reg16 REG> inline II sbc_hl_SS();
	inline II sbc_hl_hl();

	template<Reg16 REG, int EE> inline II dec_SS();
	template<Reg16 REG, int EE> inline II inc_SS();

	template<unsigned N, Reg8 REG> inline II bit_N_R();
	template<unsigned N> inline II bit_N_xhl();
	template<unsigned N> inline II bit_N_xix(unsigned a);

	template<unsigned N, Reg8 REG> inline II res_N_R();
	template<int EE> inline byte RES_X(unsigned bit, unsigned addr);
	template<unsigned N> inline II res_N_xhl();
	template<unsigned N, Reg8 REG> inline II res_N_xix_R(unsigned a);

	template<unsigned N, Reg8 REG> inline II set_N_R();
	template<int EE> inline byte SET_X(unsigned bit, unsigned addr);
	template<unsigned N> inline II set_N_xhl();
	template<unsigned N, Reg8 REG> inline II set_N_xix_R(unsigned a);

	inline byte RL(byte reg);
	template<int EE> inline byte RL_X(unsigned x);
	template<Reg8 REG> inline II rl_R();
	inline II rl_xhl();
	template<Reg8 REG> inline II rl_xix_R(unsigned a);

	inline byte RLC(byte reg);
	template<int EE> inline byte RLC_X(unsigned x);
	template<Reg8 REG> inline II rlc_R();
	inline II rlc_xhl();
	template<Reg8 REG> inline II rlc_xix_R(unsigned a);

	inline byte RR(byte reg);
	template<int EE> inline byte RR_X(unsigned x);
	template<Reg8 REG> inline II rr_R();
	inline II rr_xhl();
	template<Reg8 REG> inline II rr_xix_R(unsigned a);

	inline byte RRC(byte reg);
	template<int EE> inline byte RRC_X(unsigned x);
	template<Reg8 REG> inline II rrc_R();
	inline II rrc_xhl();
	template<Reg8 REG> inline II rrc_xix_R(unsigned a);

	inline byte SLA(byte reg);
	template<int EE> inline byte SLA_X(unsigned x);
	template<Reg8 REG> inline II sla_R();
	inline II sla_xhl();
	template<Reg8 REG> inline II sla_xix_R(unsigned a);

	inline byte SLL(byte reg);
	template<int EE> inline byte SLL_X(unsigned x);
	template<Reg8 REG> inline II sll_R();
	inline II sll_xhl();
	template<Reg8 REG> inline II sll_xix_R(unsigned a);
	inline II sll2();

	inline byte SRA(byte reg);
	template<int EE> inline byte SRA_X(unsigned x);
	template<Reg8 REG> inline II sra_R();
	inline II sra_xhl();
	template<Reg8 REG> inline II sra_xix_R(unsigned a);

	inline byte SRL(byte reg);
	template<int EE> inline byte SRL_X(unsigned x);
	template<Reg8 REG> inline II srl_R();
	inline II srl_xhl();
	template<Reg8 REG> inline II srl_xix_R(unsigned a);

	inline II rla();
	inline II rlca();
	inline II rra();
	inline II rrca();

	inline II rld();
	inline II rrd();

	template<int EE> inline void PUSH(unsigned reg);
	template<Reg16 REG, int EE> inline II push_SS();
	template<int EE> inline unsigned POP();
	template<Reg16 REG, int EE> inline II pop_SS();

	template<typename COND> inline II call(COND cond);
	template<unsigned ADDR> inline II rst();

	template<int EE, typename COND> inline II RET(COND cond);
	template<typename COND> inline II ret(COND cond);
	inline II ret();
	inline II retn();

	template<Reg16 REG, int EE> inline II jp_SS();
	template<typename COND> inline II jp(COND cond);
	template<typename COND> inline II jr(COND cond);
	inline II djnz();

	template<Reg16 REG, int EE> inline II ex_xsp_SS();

	template<Reg8 REG> inline II in_R_c();
	inline II in_a_byte();
	template<Reg8 REG> inline II out_c_R();
	inline II out_c_0();
	inline II out_byte_a();

	inline II BLOCK_CP(int increase, bool repeat);
	inline II cpd();
	inline II cpi();
	inline II cpdr();
	inline II cpir();

	inline II BLOCK_LD(int increase, bool repeat);
	inline II ldd();
	inline II ldi();
	inline II lddr();
	inline II ldir();

	inline II BLOCK_IN(int increase, bool repeat);
	inline II ind();
	inline II ini();
	inline II indr();
	inline II inir();

	inline II BLOCK_OUT(int increase, bool repeat);
	inline II outd();
	inline II outi();
	inline II otdr();
	inline II otir();

	inline II nop();
	inline II ccf();
	inline II cpl();
	inline II daa();
	inline II neg();
	inline II scf();
	inline II ex_af_af();
	inline II ex_de_hl();
	inline II exx();
	inline II di();
	inline II ei();
	inline II halt();
	template<unsigned N> inline II im_N();

	template<Reg8 REG> inline II ld_a_IR();
	inline II ld_r_a();
	inline II ld_i_a();

	template<Reg8 REG> inline II mulub_a_R();
	template<Reg16 REG> inline II muluw_hl_SS();

	friend class MSXCPU;
	friend class Z80TYPE;
	friend class R800TYPE;
};

class Z80TYPE;
class R800TYPE;
SERIALIZE_CLASS_VERSION(CPUCore<Z80TYPE>,  5);
SERIALIZE_CLASS_VERSION(CPUCore<R800TYPE>, 5); // keep these two the same

} // namespace openmsx

#endif
