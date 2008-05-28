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
	inline void next();

	inline bool cond_C();
	inline bool cond_NC();
	inline bool cond_Z();
	inline bool cond_NZ();
	inline bool cond_M();
	inline bool cond_P();
	inline bool cond_PE();
	inline bool cond_PO();

	template <CPU::Reg8 DST, CPU::Reg8 SRC> inline int ld_R_R();

	inline int ld_sp_hl();
	inline int ld_sp_ix();
	inline int ld_sp_iy();

	inline int WR_X_A(unsigned x);
	inline int ld_xbc_a();
	inline int ld_xde_a();

	template <CPU::Reg8 SRC> inline int ld_xhl_R();
	template <CPU::Reg8 SRC> inline int ld_xix_R();
	template <CPU::Reg8 SRC> inline int ld_xiy_R();

	inline int ld_xhl_byte();
	inline int ld_xix_byte();
	inline int ld_xiy_byte();

	inline int ld_xbyte_a();

	inline int WR_NN_Y(unsigned reg, int ee);
	inline int ld_xword_hl();
	inline int ld_xword_ix();
	inline int ld_xword_iy();
	inline int ld_xword_bc2();
	inline int ld_xword_de2();
	inline int ld_xword_hl2();
	inline int ld_xword_sp2();

	inline int ld_a_xbc();
	inline int ld_a_xde();

	inline int ld_a_xbyte();

	template <CPU::Reg8 DST> inline int ld_R_byte();
	template <CPU::Reg8 DST> inline int ld_R_xhl();
	template <CPU::Reg8 DST> inline int ld_R_xix();
	template <CPU::Reg8 DST> inline int ld_R_xiy();

	inline unsigned RD_P_XX(int ee);
	inline int ld_hl_xword();
	inline int ld_ix_xword();
	inline int ld_iy_xword();
	inline int ld_bc_xword2();
	inline int ld_de_xword2();
	inline int ld_hl_xword2();
	inline int ld_sp_xword2();

	inline int ld_bc_word();
	inline int ld_de_word();
	inline int ld_hl_word();
	inline int ld_ix_word();
	inline int ld_iy_word();
	inline int ld_sp_word();

	inline void ADC(byte reg);
	inline int adc_a_a();
	template <CPU::Reg8 SRC> inline int adc_a_R();
	inline int adc_a_byte();
	inline int adc_a_xhl();
	inline int adc_a_xix();
	inline int adc_a_xiy();

	inline void ADD(byte reg);
	inline int add_a_a();
	template <CPU::Reg8 SRC> inline int add_a_R();
	inline int add_a_byte();
	inline int add_a_xhl();
	inline int add_a_xix();
	inline int add_a_xiy();

	inline void AND(byte reg);
	inline int and_a();
	template <CPU::Reg8 SRC> inline int and_R();
	inline int and_byte();
	inline int and_xhl();
	inline int and_xix();
	inline int and_xiy();

	inline void CP(byte reg);
	inline int cp_a();
	template <CPU::Reg8 SRC> inline int cp_R();
	inline int cp_byte();
	inline int cp_xhl();
	inline int cp_xix();
	inline int cp_xiy();

	inline void OR(byte reg);
	inline int or_a();
	template <CPU::Reg8 SRC> inline int or_R();
	inline int or_byte();
	inline int or_xhl();
	inline int or_xix();
	inline int or_xiy();

	inline void SBC(byte reg);
	inline int sbc_a_a();
	template <CPU::Reg8 SRC> inline int sbc_a_R();
	inline int sbc_a_byte();
	inline int sbc_a_xhl();
	inline int sbc_a_xix();
	inline int sbc_a_xiy();

	inline void SUB(byte reg);
	inline int sub_a();
	template <CPU::Reg8 SRC> inline int sub_R();
	inline int sub_byte();
	inline int sub_xhl();
	inline int sub_xix();
	inline int sub_xiy();

	inline void XOR(byte reg);
	inline int xor_a();
	template <CPU::Reg8 SRC> inline int xor_R();
	inline int xor_byte();
	inline int xor_xhl();
	inline int xor_xix();
	inline int xor_xiy();

	inline byte DEC(byte reg);
	template <CPU::Reg8 REG> inline int dec_R();
	inline int DEC_X(unsigned x, int ee);
	inline int dec_xhl();
	inline int dec_xix();
	inline int dec_xiy();

	inline byte INC(byte reg);
	template <CPU::Reg8 REG> inline int inc_R();
	inline int INC_X(unsigned x, int ee);
	inline int inc_xhl();
	inline int inc_xix();
	inline int inc_xiy();

	inline int ADCW(unsigned reg);
	inline int adc_hl_bc();
	inline int adc_hl_de();
	inline int adc_hl_hl();
	inline int adc_hl_sp();

	inline unsigned ADDW(unsigned reg1, unsigned reg2);
	inline unsigned ADDW2(unsigned reg);
	inline int add_hl_bc();
	inline int add_hl_de();
	inline int add_hl_hl();
	inline int add_hl_sp();
	inline int add_ix_bc();
	inline int add_ix_de();
	inline int add_ix_ix();
	inline int add_ix_sp();
	inline int add_iy_bc();
	inline int add_iy_de();
	inline int add_iy_iy();
	inline int add_iy_sp();

	inline int SBCW(unsigned reg);
	inline int sbc_hl_bc();
	inline int sbc_hl_de();
	inline int sbc_hl_hl();
	inline int sbc_hl_sp();

	inline int dec_bc();
	inline int dec_de();
	inline int dec_hl();
	inline int dec_ix();
	inline int dec_iy();
	inline int dec_sp();

	inline int inc_bc();
	inline int inc_de();
	inline int inc_hl();
	inline int inc_ix();
	inline int inc_iy();
	inline int inc_sp();

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
	inline int push_af();
	inline int push_bc();
	inline int push_de();
	inline int push_hl();
	inline int push_ix();
	inline int push_iy();

	inline unsigned POP(int ee);
	inline int pop_af();
	inline int pop_bc();
	inline int pop_de();
	inline int pop_hl();
	inline int pop_ix();
	inline int pop_iy();

	inline int CALL();
	inline int SKIP_CALL();
	inline int call();
	inline int call_c();
	inline int call_m();
	inline int call_nc();
	inline int call_nz();
	inline int call_p();
	inline int call_pe();
	inline int call_po();
	inline int call_z();

	template <unsigned ADDR> inline int rst();

	inline int RET(bool cond, int ee);
	inline int ret();
	inline int ret_c();
	inline int ret_m();
	inline int ret_nc();
	inline int ret_nz();
	inline int ret_p();
	inline int ret_pe();
	inline int ret_po();
	inline int ret_z();
	inline int retn();

	inline int jp_hl();
	inline int jp_ix();
	inline int jp_iy();

	inline int JP();
	inline int SKIP_JP();
	inline int jp();
	inline int jp_c();
	inline int jp_m();
	inline int jp_nc();
	inline int jp_nz();
	inline int jp_p();
	inline int jp_pe();
	inline int jp_po();
	inline int jp_z();

	inline int JR(int ee);
	inline int SKIP_JR(int ee);
	inline int jr();
	inline int jr_c();
	inline int jr_nc();
	inline int jr_nz();
	inline int jr_z();
	inline int djnz();

	inline unsigned EX_SP(unsigned reg);
	inline int ex_xsp_hl();
	inline int ex_xsp_ix();
	inline int ex_xsp_iy();

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

	inline int ld_a_i();
	inline int ld_a_r();
	inline int ld_i_a();
	inline int ld_r_a();

	template <CPU::Reg8 REG> inline int mulub_a_R();
	inline int MULUW(unsigned reg);
	inline int muluw_hl_bc();
	inline int muluw_hl_sp();

	inline int nn_cb(unsigned reg);
	inline int dd_cb();
	inline int fd_cb();
	inline int cb();
	inline int ed();
	inline int dd();
	inline int fd();

	friend class MSXCPU;
	friend class Z80TYPE;
	friend class R800TYPE;
};

} // namespace openmsx

#endif
