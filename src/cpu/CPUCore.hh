// $Id$

#ifndef CPUCORE_HH
#define CPUCORE_HH

#include "openmsx.hh"
#include "Observer.hh"
#include "CPU.hh"
#include "CacheLine.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXCPUInterface;
class Scheduler;
class MSXMotherBoard;
class EmuTime;
class BooleanSetting;
class IntegerSetting;
class Setting;

template <class CPU_POLICY>
class CPUCore : private CPU_POLICY, public CPU, private Observer<Setting>
{
public:
	CPUCore(MSXMotherBoard& motherboard, const std::string& name,
	        const BooleanSetting& traceSetting, const EmuTime& time);
	virtual ~CPUCore();

	void setInterface(MSXCPUInterface* interf);

	/**
	 * Reset the CPU.
	 */
	void doReset(const EmuTime& time);

	virtual void execute();
	virtual void exitCPULoopSync();
	virtual void exitCPULoopAsync();
	virtual void warp(const EmuTime& time);
	virtual const EmuTime& getCurrentTime() const;
	virtual void wait(const EmuTime& time);
	virtual void waitCycles(unsigned cycles);
	virtual void setNextSyncPoint(const EmuTime& time);
	virtual void invalidateMemCache(unsigned start, unsigned size);
	virtual void doStep();
	virtual void doContinue();
	virtual void doBreak();

	virtual void disasmCommand(const std::vector<TclObject*>& tokens,
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void doContinue2();
	bool needExitCPULoop();
	void setSlowInstructions();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// memory cache
	const byte* readCacheLine[CacheLine::NUM];
	byte* writeCacheLine[CacheLine::NUM];
	bool readCacheTried [CacheLine::NUM];
	bool writeCacheTried[CacheLine::NUM];

	MSXMotherBoard& motherboard;
	Scheduler& scheduler;
	MSXCPUInterface* interface;

	const BooleanSetting& traceSetting;

	// dynamic freq
	std::auto_ptr<BooleanSetting> freqLocked;
	std::auto_ptr<IntegerSetting> freqValue;
	unsigned freq;

	// state machine variables
	int slowInstructions;
	int NMIStatus;
	int IRQStatus;

	unsigned memptr;

	/**
	 * Set to true when there was a rising edge on the NMI line
	 * (rising = non-active -> active).
	 * Set to false when the CPU jumps to the NMI handler address.
	 */
	bool nmiEdge;

	volatile bool exitLoop;


	inline void cpuTracePre();
	inline void cpuTracePost();
	void cpuTracePost_slow();

	inline byte READ_PORT(unsigned port, unsigned cc);
	inline void WRITE_PORT(unsigned port, byte value, unsigned cc);

	template <bool PRE_PB, bool POST_PB>
	byte RDMEMslow(unsigned address, unsigned cc);
	template <bool PRE_PB, bool POST_PB>
	inline byte RDMEM_impl(unsigned address, unsigned cc);
	inline byte RDMEM_OPCODE(unsigned cc);
	inline byte RDMEM(unsigned address, unsigned cc);

	template <bool PRE_PB, bool POST_PB>
	unsigned RD_WORD_slow(unsigned address, unsigned cc);
	template <bool PRE_PB, bool POST_PB>
	inline unsigned RD_WORD_impl(unsigned address, unsigned cc);
	inline unsigned RD_WORD_PC(unsigned cc);
	inline unsigned RD_WORD(unsigned address, unsigned cc);

	template <bool PRE_PB, bool POST_PB>
	void WRMEMslow(unsigned address, byte value, unsigned cc);
	template <bool PRE_PB, bool POST_PB>
	inline void WRMEM_impl(unsigned address, byte value, unsigned cc);
	inline void WRMEM(unsigned address, byte value, unsigned cc);

	void WR_WORD_slow(unsigned address, unsigned value, unsigned cc);
	inline void WR_WORD(unsigned address, unsigned value, unsigned cc);

	template <bool PRE_PB, bool POST_PB>
	void WR_WORD_rev_slow(unsigned address, unsigned value, unsigned cc);
	template <bool PRE_PB, bool POST_PB>
	inline void WR_WORD_rev(unsigned address, unsigned value, unsigned cc);

	inline void M1Cycle();
	int executeInstruction1_slow(byte opcode);
	inline int executeInstruction1(byte opcode);
	inline void nmi();
	inline void irq0();
	inline void irq1();
	inline void irq2();
	void executeFast();
	inline void executeFastInline();
	void executeSlow();

	template <CPU::Reg8 DST, CPU::Reg8 SRC> inline int ld_R_R();
	template <CPU::Reg16 REG> inline int ld_sp_SS();
	template <CPU::Reg16 REG> inline int ld_SS_a();
	template <CPU::Reg8 SRC> inline int ld_xhl_R();
	template <CPU::Reg16 IXY, CPU::Reg8 SRC> inline int ld_xix_R();

	inline int ld_xhl_byte();
	template <CPU::Reg16 IXY> inline int ld_xix_byte();

	inline int WR_NN_Y(unsigned reg, int ee);
	template <CPU::Reg16 REG> inline int ld_xword_SS();
	template <CPU::Reg16 REG> inline int ld_xword_SS_ED();
	template <CPU::Reg16 REG> inline int ld_a_SS();

	inline int ld_xbyte_a();
	inline int ld_a_xbyte();

	template <CPU::Reg8 DST> inline int ld_R_byte();
	template <CPU::Reg8 DST> inline int ld_R_xhl();
	template <CPU::Reg8 DST, CPU::Reg16 IXY> inline int ld_R_xix();

	inline unsigned RD_P_XX(int ee);
	template <CPU::Reg16 REG> inline int ld_SS_xword();
	template <CPU::Reg16 REG> inline int ld_SS_xword_ED();

	template <CPU::Reg16 REG> inline int ld_SS_word();

	inline void ADC(byte reg);
	inline int adc_a_a();
	template <CPU::Reg8 SRC> inline int adc_a_R();
	inline int adc_a_byte();
	inline int adc_a_xhl();
	template <CPU::Reg16 IXY> inline int adc_a_xix();

	inline void ADD(byte reg);
	inline int add_a_a();
	template <CPU::Reg8 SRC> inline int add_a_R();
	inline int add_a_byte();
	inline int add_a_xhl();
	template <CPU::Reg16 IXY> inline int add_a_xix();

	inline void AND(byte reg);
	inline int and_a();
	template <CPU::Reg8 SRC> inline int and_R();
	inline int and_byte();
	inline int and_xhl();
	template <CPU::Reg16 IXY> inline int and_xix();

	inline void CP(byte reg);
	inline int cp_a();
	template <CPU::Reg8 SRC> inline int cp_R();
	inline int cp_byte();
	inline int cp_xhl();
	template <CPU::Reg16 IXY> inline int cp_xix();

	inline void OR(byte reg);
	inline int or_a();
	template <CPU::Reg8 SRC> inline int or_R();
	inline int or_byte();
	inline int or_xhl();
	template <CPU::Reg16 IXY> inline int or_xix();

	inline void SBC(byte reg);
	inline int sbc_a_a();
	template <CPU::Reg8 SRC> inline int sbc_a_R();
	inline int sbc_a_byte();
	inline int sbc_a_xhl();
	template <CPU::Reg16 IXY> inline int sbc_a_xix();

	inline void SUB(byte reg);
	inline int sub_a();
	template <CPU::Reg8 SRC> inline int sub_R();
	inline int sub_byte();
	inline int sub_xhl();
	template <CPU::Reg16 IXY> inline int sub_xix();

	inline void XOR(byte reg);
	inline int xor_a();
	template <CPU::Reg8 SRC> inline int xor_R();
	inline int xor_byte();
	inline int xor_xhl();
	template <CPU::Reg16 IXY> inline int xor_xix();

	inline byte DEC(byte reg);
	template <CPU::Reg8 REG> inline int dec_R();
	inline int DEC_X(unsigned x, int ee);
	inline int dec_xhl();
	template <CPU::Reg16 IXY> inline int dec_xix();

	inline byte INC(byte reg);
	template <CPU::Reg8 REG> inline int inc_R();
	inline int INC_X(unsigned x, int ee);
	inline int inc_xhl();
	template <CPU::Reg16 IXY> inline int inc_xix();

	template <CPU::Reg16 REG> inline int adc_hl_SS();
	inline int adc_hl_hl();
	template <CPU::Reg16 REG1, CPU::Reg16 REG2> inline int add_SS_TT();
	template <CPU::Reg16 REG> inline int add_SS_SS();
	template <CPU::Reg16 REG> inline int sbc_hl_SS();
	inline int sbc_hl_hl();

	template <CPU::Reg16 REG> inline int dec_SS();
	template <CPU::Reg16 REG> inline int inc_SS();

	template <unsigned N, CPU::Reg8 REG> inline int bit_N_R();
	template <unsigned N> inline int bit_N_xhl();
	template <unsigned N> inline int bit_N_xix(unsigned a);

	template <unsigned N, CPU::Reg8 REG> inline int res_N_R();
	inline byte RES_X(unsigned bit, unsigned addr, int ee);
	template <unsigned N> inline int res_N_xhl();
	template <unsigned N, CPU::Reg8 REG> inline int res_N_xix_R(unsigned a);

	template <unsigned N, CPU::Reg8 REG> inline int set_N_R();
	inline byte SET_X(unsigned bit, unsigned addr, int ee);
	template <unsigned N> inline int set_N_xhl();
	template <unsigned N, CPU::Reg8 REG> inline int set_N_xix_R(unsigned a);

	inline byte RL(byte reg);
	inline byte RL_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int rl_R();
	inline int rl_xhl();
	template <CPU::Reg8 REG> inline int rl_xix_R(unsigned a);

	inline byte RLC(byte reg);
	inline byte RLC_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int rlc_R();
	inline int rlc_xhl();
	template <CPU::Reg8 REG> inline int rlc_xix_R(unsigned a);

	inline byte RR(byte reg);
	inline byte RR_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int rr_R();
	inline int rr_xhl();
	template <CPU::Reg8 REG> inline int rr_xix_R(unsigned a);

	inline byte RRC(byte reg);
	inline byte RRC_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int rrc_R();
	inline int rrc_xhl();
	template <CPU::Reg8 REG> inline int rrc_xix_R(unsigned a);

	inline byte SLA(byte reg);
	inline byte SLA_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int sla_R();
	inline int sla_xhl();
	template <CPU::Reg8 REG> inline int sla_xix_R(unsigned a);

	inline byte SLL(byte reg);
	inline byte SLL_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int sll_R();
	inline int sll_xhl();
	template <CPU::Reg8 REG> inline int sll_xix_R(unsigned a);

	inline byte SRA(byte reg);
	inline byte SRA_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int sra_R();
	inline int sra_xhl();
	template <CPU::Reg8 REG> inline int sra_xix_R(unsigned a);

	inline byte SRL(byte reg);
	inline byte SRL_X(unsigned x, int ee);
	template <CPU::Reg8 REG> inline int srl_R();
	inline int srl_xhl();
	template <CPU::Reg8 REG> inline int srl_xix_R(unsigned a);

	inline int rla();
	inline int rlca();
	inline int rra();
	inline int rrca();

	inline int rld();
	inline int rrd();

	inline void PUSH(unsigned reg, int ee);
	template <CPU::Reg16 REG> inline int push_SS();
	inline unsigned POP(int ee);
	template <CPU::Reg16 REG> inline int pop_SS();

	template <typename COND> inline int call(COND cond);
	template <unsigned ADDR> inline int rst();

	template <typename COND> inline int RET(COND cond, int ee);
	template <typename COND> inline int ret(COND cond);
	inline int ret();
	inline int retn();

	template <CPU::Reg16 REG> inline int jp_SS();
	template <typename COND> inline int jp(COND cond);
	template <typename COND> inline int jr(COND cond);
	inline int djnz();

	template <CPU::Reg16 REG> inline int ex_xsp_SS();

	template <CPU::Reg8 REG> inline int in_R_c();
	inline int in_a_byte();
	template <CPU::Reg8 REG> inline int out_c_R();
	inline int out_byte_a();

	inline int BLOCK_CP(int increase, bool repeat);
	inline int cpd();
	inline int cpi();
	inline int cpdr();
	inline int cpir();

	inline int BLOCK_LD(int increase, bool repeat);
	inline int ldd();
	inline int ldi();
	inline int lddr();
	inline int ldir();

	inline int BLOCK_IN(int increase, bool repeat);
	inline int ind();
	inline int ini();
	inline int indr();
	inline int inir();

	inline int BLOCK_OUT(int increase, bool repeat);
	inline int outd();
	inline int outi();
	inline int otdr();
	inline int otir();

	inline int nop();
	inline int ccf();
	inline int cpl();
	inline int daa();
	inline int neg();
	inline int scf();
	inline int ex_af_af();
	inline int ex_de_hl();
	inline int exx();
	inline int di();
	inline int ei();
	inline int halt();
	template <unsigned N> inline int im_N();

	template <CPU::Reg8 REG> inline int ld_a_IR();
	template <CPU::Reg8 REG> inline int ld_IR_a();

	template <CPU::Reg8 REG> int mulub_a_R();
	template <CPU::Reg16 REG> int muluw_hl_SS();

	inline int cb();
	inline int ed();
	template <CPU::Reg16 IXY> inline int xy_cb();
	template <CPU::Reg16 IXY, CPU::Reg8 IXYH, CPU::Reg8 IXYL> inline int xy();

	friend class MSXCPU;
	friend class Z80TYPE;
	friend class R800TYPE;
};

} // namespace openmsx

#endif
