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
	// shorthand (for self) because it's used very often hereafter
	typedef CPUCore S;
	// Use function-pointer to a static function that takes a 'this'
	// argument instead of a function-to-member-pointer.
	// In pure CPU benchmark (zexall with none renderer) this was up to
	// 20% (twenty!) faster!
	typedef void (*FuncPtr)(S&);
	typedef void (*FuncPtrN)(S&, int);

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

	inline byte READ_PORT(unsigned port);
	inline void WRITE_PORT(unsigned port, byte value);
	byte RDMEMslow(unsigned address);
	byte RDMEM_OPCODEslow(unsigned address);
	void WRMEMslow(unsigned address, byte value);
	inline byte RDMEM_OPCODE();
	inline unsigned RD_WORD_PC();
	unsigned RD_WORD_PC_slow();
	inline unsigned RD_WORD(unsigned address);
	unsigned RD_WORD_slow(unsigned address);
	inline byte RDMEM(unsigned address);
	inline void WRMEM(unsigned address, byte value);
	inline void WR_WORD(unsigned address, unsigned value);
	void WR_WORD_slow(unsigned address, unsigned value);
	inline void WR_WORD_rev(unsigned address, unsigned value);
	void WR_WORD_rev_slow(unsigned address, unsigned value);

	inline void M1Cycle();
	inline void executeInstruction1(byte opcode);
	inline void nmi();
	inline void irq0();
	inline void irq1();
	inline void irq2();
	inline void executeFast();
	void executeSlow();
	inline void next();

	inline bool C();
	inline bool NC();
	inline bool Z();
	inline bool NZ();
	inline bool M();
	inline bool P();
	inline bool PE();
	inline bool PO();

	static const FuncPtrN opcode_dd_cb[256];
	static const FuncPtrN opcode_fd_cb[256];
	static const FuncPtr opcode_cb[256];
	static const FuncPtr opcode_ed[256];
	static const FuncPtr opcode_dd[256];
	static const FuncPtr opcode_fd[256];
	static const FuncPtr opcode_main[256];

	static void ld_a_b(S& s);
	static void ld_a_c(S& s);
	static void ld_a_d(S& s);
	static void ld_a_e(S& s);
	static void ld_a_h(S& s);
	static void ld_a_l(S& s);
	static void ld_a_ixh(S& s);
	static void ld_a_ixl(S& s);
	static void ld_a_iyh(S& s);
	static void ld_a_iyl(S& s);
	static void ld_b_a(S& s);
	static void ld_b_c(S& s);
	static void ld_b_d(S& s);
	static void ld_b_e(S& s);
	static void ld_b_h(S& s);
	static void ld_b_l(S& s);
	static void ld_b_ixh(S& s);
	static void ld_b_ixl(S& s);
	static void ld_b_iyh(S& s);
	static void ld_b_iyl(S& s);
	static void ld_c_a(S& s);
	static void ld_c_b(S& s);
	static void ld_c_d(S& s);
	static void ld_c_e(S& s);
	static void ld_c_h(S& s);
	static void ld_c_l(S& s);
	static void ld_c_ixh(S& s);
	static void ld_c_ixl(S& s);
	static void ld_c_iyh(S& s);
	static void ld_c_iyl(S& s);
	static void ld_d_a(S& s);
	static void ld_d_c(S& s);
	static void ld_d_b(S& s);
	static void ld_d_e(S& s);
	static void ld_d_h(S& s);
	static void ld_d_l(S& s);
	static void ld_d_ixh(S& s);
	static void ld_d_ixl(S& s);
	static void ld_d_iyh(S& s);
	static void ld_d_iyl(S& s);
	static void ld_e_a(S& s);
	static void ld_e_c(S& s);
	static void ld_e_b(S& s);
	static void ld_e_d(S& s);
	static void ld_e_h(S& s);
	static void ld_e_l(S& s);
	static void ld_e_ixh(S& s);
	static void ld_e_ixl(S& s);
	static void ld_e_iyh(S& s);
	static void ld_e_iyl(S& s);
	static void ld_h_a(S& s);
	static void ld_h_c(S& s);
	static void ld_h_b(S& s);
	static void ld_h_e(S& s);
	static void ld_h_d(S& s);
	static void ld_h_l(S& s);
	static void ld_l_a(S& s);
	static void ld_l_c(S& s);
	static void ld_l_b(S& s);
	static void ld_l_e(S& s);
	static void ld_l_d(S& s);
	static void ld_l_h(S& s);
	static void ld_ixh_a(S& s);
	static void ld_ixh_b(S& s);
	static void ld_ixh_c(S& s);
	static void ld_ixh_d(S& s);
	static void ld_ixh_e(S& s);
	static void ld_ixh_ixl(S& s);
	static void ld_ixl_a(S& s);
	static void ld_ixl_b(S& s);
	static void ld_ixl_c(S& s);
	static void ld_ixl_d(S& s);
	static void ld_ixl_e(S& s);
	static void ld_ixl_ixh(S& s);
	static void ld_iyh_a(S& s);
	static void ld_iyh_b(S& s);
	static void ld_iyh_c(S& s);
	static void ld_iyh_d(S& s);
	static void ld_iyh_e(S& s);
	static void ld_iyh_iyl(S& s);
	static void ld_iyl_a(S& s);
	static void ld_iyl_b(S& s);
	static void ld_iyl_c(S& s);
	static void ld_iyl_d(S& s);
	static void ld_iyl_e(S& s);
	static void ld_iyl_iyh(S& s);

	static void ld_sp_hl(S& s);
	static void ld_sp_ix(S& s);
	static void ld_sp_iy(S& s);

	inline void WR_X_A(unsigned x);
	static void ld_xbc_a(S& s);
	static void ld_xde_a(S& s);
	static void ld_xhl_a(S& s);

	inline void WR_HL_X(byte val);
	static void ld_xhl_b(S& s);
	static void ld_xhl_c(S& s);
	static void ld_xhl_d(S& s);
	static void ld_xhl_e(S& s);
	static void ld_xhl_h(S& s);
	static void ld_xhl_l(S& s);

	static void ld_xhl_byte(S& s);
	inline void WR_X_X(byte val);
	inline void WR_XIX(byte val);
	static void ld_xix_a(S& s);
	static void ld_xix_b(S& s);
	static void ld_xix_c(S& s);
	static void ld_xix_d(S& s);
	static void ld_xix_e(S& s);
	static void ld_xix_h(S& s);
	static void ld_xix_l(S& s);

	inline void WR_XIY(byte val);
	static void ld_xiy_a(S& s);
	static void ld_xiy_b(S& s);
	static void ld_xiy_c(S& s);
	static void ld_xiy_d(S& s);
	static void ld_xiy_e(S& s);
	static void ld_xiy_h(S& s);
	static void ld_xiy_l(S& s);

	static void ld_xix_byte(S& s);
	static void ld_xiy_byte(S& s);

	static void ld_xbyte_a(S& s);

	inline void WR_NN_Y(unsigned reg);
	static void ld_xword_bc(S& s);
	static void ld_xword_de(S& s);
	static void ld_xword_hl(S& s);
	static void ld_xword_ix(S& s);
	static void ld_xword_iy(S& s);
	static void ld_xword_sp(S& s);

	inline void RD_A_X(unsigned x);
	static void ld_a_xbc(S& s);
	static void ld_a_xde(S& s);
	static void ld_a_xhl(S& s);

	static void ld_a_xix(S& s);
	static void ld_a_xiy(S& s);

	static void ld_a_xbyte(S& s);

	static void ld_a_byte(S& s);
	static void ld_b_byte(S& s);
	static void ld_c_byte(S& s);
	static void ld_d_byte(S& s);
	static void ld_e_byte(S& s);
	static void ld_h_byte(S& s);
	static void ld_l_byte(S& s);
	static void ld_ixh_byte(S& s);
	static void ld_ixl_byte(S& s);
	static void ld_iyh_byte(S& s);
	static void ld_iyl_byte(S& s);

	static void ld_b_xhl(S& s);
	static void ld_c_xhl(S& s);
	static void ld_d_xhl(S& s);
	static void ld_e_xhl(S& s);
	static void ld_h_xhl(S& s);
	static void ld_l_xhl(S& s);

	inline byte RD_R_XIX();
	static void ld_b_xix(S& s);
	static void ld_c_xix(S& s);
	static void ld_d_xix(S& s);
	static void ld_e_xix(S& s);
	static void ld_h_xix(S& s);
	static void ld_l_xix(S& s);

	inline byte RD_R_XIY();
	static void ld_b_xiy(S& s);
	static void ld_c_xiy(S& s);
	static void ld_d_xiy(S& s);
	static void ld_e_xiy(S& s);
	static void ld_h_xiy(S& s);
	static void ld_l_xiy(S& s);

	inline unsigned RD_P_XX();
	static void ld_bc_xword(S& s);
	static void ld_de_xword(S& s);
	static void ld_hl_xword(S& s);
	static void ld_ix_xword(S& s);
	static void ld_iy_xword(S& s);
	static void ld_sp_xword(S& s);

	static void ld_bc_word(S& s);
	static void ld_de_word(S& s);
	static void ld_hl_word(S& s);
	static void ld_ix_word(S& s);
	static void ld_iy_word(S& s);
	static void ld_sp_word(S& s);

	inline void ADC(byte reg);
	static void adc_a_a(S& s);
	static void adc_a_b(S& s);
	static void adc_a_c(S& s);
	static void adc_a_d(S& s);
	static void adc_a_e(S& s);
	static void adc_a_h(S& s);
	static void adc_a_l(S& s);
	static void adc_a_ixl(S& s);
	static void adc_a_ixh(S& s);
	static void adc_a_iyl(S& s);
	static void adc_a_iyh(S& s);
	static void adc_a_byte(S& s);
	inline void adc_a_x(unsigned x);
	static void adc_a_xhl(S& s);
	static void adc_a_xix(S& s);
	static void adc_a_xiy(S& s);

	inline void ADD(byte reg);
	static void add_a_a(S& s);
	static void add_a_b(S& s);
	static void add_a_c(S& s);
	static void add_a_d(S& s);
	static void add_a_e(S& s);
	static void add_a_h(S& s);
	static void add_a_l(S& s);
	static void add_a_ixl(S& s);
	static void add_a_ixh(S& s);
	static void add_a_iyl(S& s);
	static void add_a_iyh(S& s);
	static void add_a_byte(S& s);
	inline void add_a_x(unsigned x);
	static void add_a_xhl(S& s);
	static void add_a_xix(S& s);
	static void add_a_xiy(S& s);

	inline void AND(byte reg);
	static void and_a(S& s);
	static void and_b(S& s);
	static void and_c(S& s);
	static void and_d(S& s);
	static void and_e(S& s);
	static void and_h(S& s);
	static void and_l(S& s);
	static void and_ixh(S& s);
	static void and_ixl(S& s);
	static void and_iyh(S& s);
	static void and_iyl(S& s);
	static void and_byte(S& s);
	inline void and_x(unsigned x);
	static void and_xhl(S& s);
	static void and_xix(S& s);
	static void and_xiy(S& s);

	inline void CP(byte reg);
	static void cp_a(S& s);
	static void cp_b(S& s);
	static void cp_c(S& s);
	static void cp_d(S& s);
	static void cp_e(S& s);
	static void cp_h(S& s);
	static void cp_l(S& s);
	static void cp_ixh(S& s);
	static void cp_ixl(S& s);
	static void cp_iyh(S& s);
	static void cp_iyl(S& s);
	static void cp_byte(S& s);
	inline void cp_x(unsigned x);
	static void cp_xhl(S& s);
	static void cp_xix(S& s);
	static void cp_xiy(S& s);

	inline void OR(byte reg);
	static void or_a(S& s);
	static void or_b(S& s);
	static void or_c(S& s);
	static void or_d(S& s);
	static void or_e(S& s);
	static void or_h(S& s);
	static void or_l(S& s);
	static void or_ixh(S& s);
	static void or_ixl(S& s);
	static void or_iyh(S& s);
	static void or_iyl(S& s);
	static void or_byte(S& s);
	inline void or_x(unsigned x);
	static void or_xhl(S& s);
	static void or_xix(S& s);
	static void or_xiy(S& s);

	inline void SBC(byte reg);
	static void sbc_a_a(S& s);
	static void sbc_a_b(S& s);
	static void sbc_a_c(S& s);
	static void sbc_a_d(S& s);
	static void sbc_a_e(S& s);
	static void sbc_a_h(S& s);
	static void sbc_a_l(S& s);
	static void sbc_a_ixh(S& s);
	static void sbc_a_ixl(S& s);
	static void sbc_a_iyh(S& s);
	static void sbc_a_iyl(S& s);
	static void sbc_a_byte(S& s);
	inline void sbc_a_x(unsigned x);
	static void sbc_a_xhl(S& s);
	static void sbc_a_xix(S& s);
	static void sbc_a_xiy(S& s);

	inline void SUB(byte reg);
	static void sub_a(S& s);
	static void sub_b(S& s);
	static void sub_c(S& s);
	static void sub_d(S& s);
	static void sub_e(S& s);
	static void sub_h(S& s);
	static void sub_l(S& s);
	static void sub_ixh(S& s);
	static void sub_ixl(S& s);
	static void sub_iyh(S& s);
	static void sub_iyl(S& s);
	static void sub_byte(S& s);
	inline void sub_x(unsigned x);
	static void sub_xhl(S& s);
	static void sub_xix(S& s);
	static void sub_xiy(S& s);

	inline void XOR(byte reg);
	static void xor_a(S& s);
	static void xor_b(S& s);
	static void xor_c(S& s);
	static void xor_d(S& s);
	static void xor_e(S& s);
	static void xor_h(S& s);
	static void xor_l(S& s);
	static void xor_ixh(S& s);
	static void xor_ixl(S& s);
	static void xor_iyh(S& s);
	static void xor_iyl(S& s);
	static void xor_byte(S& s);
	inline void xor_x(unsigned x);
	static void xor_xhl(S& s);
	static void xor_xix(S& s);
	static void xor_xiy(S& s);

	inline byte DEC(byte reg);
	static void dec_a(S& s);
	static void dec_b(S& s);
	static void dec_c(S& s);
	static void dec_d(S& s);
	static void dec_e(S& s);
	static void dec_h(S& s);
	static void dec_l(S& s);
	static void dec_ixh(S& s);
	static void dec_ixl(S& s);
	static void dec_iyh(S& s);
	static void dec_iyl(S& s);
	inline void DEC_X(unsigned x);
	static void dec_xhl(S& s);
	static void dec_xix(S& s);
	static void dec_xiy(S& s);

	inline byte INC(byte reg);
	static void inc_a(S& s);
	static void inc_b(S& s);
	static void inc_c(S& s);
	static void inc_d(S& s);
	static void inc_e(S& s);
	static void inc_h(S& s);
	static void inc_l(S& s);
	static void inc_ixh(S& s);
	static void inc_ixl(S& s);
	static void inc_iyh(S& s);
	static void inc_iyl(S& s);
	inline void INC_X(unsigned x);
	static void inc_xhl(S& s);
	static void inc_xix(S& s);
	static void inc_xiy(S& s);

	inline void ADCW(unsigned reg);
	static void adc_hl_bc(S& s);
	static void adc_hl_de(S& s);
	static void adc_hl_hl(S& s);
	static void adc_hl_sp(S& s);

	inline unsigned ADDW(unsigned reg1, unsigned reg2);
	inline unsigned ADDW2(unsigned reg);
	static void add_hl_bc(S& s);
	static void add_hl_de(S& s);
	static void add_hl_hl(S& s);
	static void add_hl_sp(S& s);
	static void add_ix_bc(S& s);
	static void add_ix_de(S& s);
	static void add_ix_ix(S& s);
	static void add_ix_sp(S& s);
	static void add_iy_bc(S& s);
	static void add_iy_de(S& s);
	static void add_iy_iy(S& s);
	static void add_iy_sp(S& s);

	inline void SBCW(unsigned reg);
	static void sbc_hl_bc(S& s);
	static void sbc_hl_de(S& s);
	static void sbc_hl_hl(S& s);
	static void sbc_hl_sp(S& s);

	static void dec_bc(S& s);
	static void dec_de(S& s);
	static void dec_hl(S& s);
	static void dec_ix(S& s);
	static void dec_iy(S& s);
	static void dec_sp(S& s);

	static void inc_bc(S& s);
	static void inc_de(S& s);
	static void inc_hl(S& s);
	static void inc_ix(S& s);
	static void inc_iy(S& s);
	static void inc_sp(S& s);

	inline void BIT(byte bit, byte reg);
	static void bit_0_a(S& s);
	static void bit_0_b(S& s);
	static void bit_0_c(S& s);
	static void bit_0_d(S& s);
	static void bit_0_e(S& s);
	static void bit_0_h(S& s);
	static void bit_0_l(S& s);
	static void bit_1_a(S& s);
	static void bit_1_b(S& s);
	static void bit_1_c(S& s);
	static void bit_1_d(S& s);
	static void bit_1_e(S& s);
	static void bit_1_h(S& s);
	static void bit_1_l(S& s);
	static void bit_2_a(S& s);
	static void bit_2_b(S& s);
	static void bit_2_c(S& s);
	static void bit_2_d(S& s);
	static void bit_2_e(S& s);
	static void bit_2_h(S& s);
	static void bit_2_l(S& s);
	static void bit_3_a(S& s);
	static void bit_3_b(S& s);
	static void bit_3_c(S& s);
	static void bit_3_d(S& s);
	static void bit_3_e(S& s);
	static void bit_3_h(S& s);
	static void bit_3_l(S& s);
	static void bit_4_a(S& s);
	static void bit_4_b(S& s);
	static void bit_4_c(S& s);
	static void bit_4_d(S& s);
	static void bit_4_e(S& s);
	static void bit_4_h(S& s);
	static void bit_4_l(S& s);
	static void bit_5_a(S& s);
	static void bit_5_b(S& s);
	static void bit_5_c(S& s);
	static void bit_5_d(S& s);
	static void bit_5_e(S& s);
	static void bit_5_h(S& s);
	static void bit_5_l(S& s);
	static void bit_6_a(S& s);
	static void bit_6_b(S& s);
	static void bit_6_c(S& s);
	static void bit_6_d(S& s);
	static void bit_6_e(S& s);
	static void bit_6_h(S& s);
	static void bit_6_l(S& s);
	static void bit_7_a(S& s);
	static void bit_7_b(S& s);
	static void bit_7_c(S& s);
	static void bit_7_d(S& s);
	static void bit_7_e(S& s);
	static void bit_7_h(S& s);
	static void bit_7_l(S& s);

	inline void BIT_HL(byte bit);
	static void bit_0_xhl(S& s);
	static void bit_1_xhl(S& s);
	static void bit_2_xhl(S& s);
	static void bit_3_xhl(S& s);
	static void bit_4_xhl(S& s);
	static void bit_5_xhl(S& s);
	static void bit_6_xhl(S& s);
	static void bit_7_xhl(S& s);

	inline void BIT_IX(byte bit, int ofst);
	static void bit_0_xix(S& s, int o);
	static void bit_1_xix(S& s, int o);
	static void bit_2_xix(S& s, int o);
	static void bit_3_xix(S& s, int o);
	static void bit_4_xix(S& s, int o);
	static void bit_5_xix(S& s, int o);
	static void bit_6_xix(S& s, int o);
	static void bit_7_xix(S& s, int o);

	inline void BIT_IY(byte bit, int ofst);
	static void bit_0_xiy(S& s, int o);
	static void bit_1_xiy(S& s, int o);
	static void bit_2_xiy(S& s, int o);
	static void bit_3_xiy(S& s, int o);
	static void bit_4_xiy(S& s, int o);
	static void bit_5_xiy(S& s, int o);
	static void bit_6_xiy(S& s, int o);
	static void bit_7_xiy(S& s, int o);

	inline byte RES(unsigned bit, byte reg);
	static void res_0_a(S& s);
	static void res_0_b(S& s);
	static void res_0_c(S& s);
	static void res_0_d(S& s);
	static void res_0_e(S& s);
	static void res_0_h(S& s);
	static void res_0_l(S& s);
	static void res_1_a(S& s);
	static void res_1_b(S& s);
	static void res_1_c(S& s);
	static void res_1_d(S& s);
	static void res_1_e(S& s);
	static void res_1_h(S& s);
	static void res_1_l(S& s);
	static void res_2_a(S& s);
	static void res_2_b(S& s);
	static void res_2_c(S& s);
	static void res_2_d(S& s);
	static void res_2_e(S& s);
	static void res_2_h(S& s);
	static void res_2_l(S& s);
	static void res_3_a(S& s);
	static void res_3_b(S& s);
	static void res_3_c(S& s);
	static void res_3_d(S& s);
	static void res_3_e(S& s);
	static void res_3_h(S& s);
	static void res_3_l(S& s);
	static void res_4_a(S& s);
	static void res_4_b(S& s);
	static void res_4_c(S& s);
	static void res_4_d(S& s);
	static void res_4_e(S& s);
	static void res_4_h(S& s);
	static void res_4_l(S& s);
	static void res_5_a(S& s);
	static void res_5_b(S& s);
	static void res_5_c(S& s);
	static void res_5_d(S& s);
	static void res_5_e(S& s);
	static void res_5_h(S& s);
	static void res_5_l(S& s);
	static void res_6_a(S& s);
	static void res_6_b(S& s);
	static void res_6_c(S& s);
	static void res_6_d(S& s);
	static void res_6_e(S& s);
	static void res_6_h(S& s);
	static void res_6_l(S& s);
	static void res_7_a(S& s);
	static void res_7_b(S& s);
	static void res_7_c(S& s);
	static void res_7_d(S& s);
	static void res_7_e(S& s);
	static void res_7_h(S& s);
	static void res_7_l(S& s);

	inline byte RES_X(unsigned bit, unsigned x);
	inline byte RES_X_(unsigned bit, unsigned x);
	inline byte RES_X_X(unsigned bit, int ofst);
	inline byte RES_X_Y(unsigned bit, int ofst);
	static void res_0_xhl(S& s);
	static void res_1_xhl(S& s);
	static void res_2_xhl(S& s);
	static void res_3_xhl(S& s);
	static void res_4_xhl(S& s);
	static void res_5_xhl(S& s);
	static void res_6_xhl(S& s);
	static void res_7_xhl(S& s);
	static void res_0_xix(S& s, int o);
	static void res_1_xix(S& s, int o);
	static void res_2_xix(S& s, int o);
	static void res_3_xix(S& s, int o);
	static void res_4_xix(S& s, int o);
	static void res_5_xix(S& s, int o);
	static void res_6_xix(S& s, int o);
	static void res_7_xix(S& s, int o);
	static void res_0_xiy(S& s, int o);
	static void res_1_xiy(S& s, int o);
	static void res_2_xiy(S& s, int o);
	static void res_3_xiy(S& s, int o);
	static void res_4_xiy(S& s, int o);
	static void res_5_xiy(S& s, int o);
	static void res_6_xiy(S& s, int o);
	static void res_7_xiy(S& s, int o);
	static void res_0_xix_a(S& s, int o);
	static void res_0_xix_b(S& s, int o);
	static void res_0_xix_c(S& s, int o);
	static void res_0_xix_d(S& s, int o);
	static void res_0_xix_e(S& s, int o);
	static void res_0_xix_h(S& s, int o);
	static void res_0_xix_l(S& s, int o);
	static void res_1_xix_a(S& s, int o);
	static void res_1_xix_b(S& s, int o);
	static void res_1_xix_c(S& s, int o);
	static void res_1_xix_d(S& s, int o);
	static void res_1_xix_e(S& s, int o);
	static void res_1_xix_h(S& s, int o);
	static void res_1_xix_l(S& s, int o);
	static void res_2_xix_a(S& s, int o);
	static void res_2_xix_b(S& s, int o);
	static void res_2_xix_c(S& s, int o);
	static void res_2_xix_d(S& s, int o);
	static void res_2_xix_e(S& s, int o);
	static void res_2_xix_h(S& s, int o);
	static void res_2_xix_l(S& s, int o);
	static void res_3_xix_a(S& s, int o);
	static void res_3_xix_b(S& s, int o);
	static void res_3_xix_c(S& s, int o);
	static void res_3_xix_d(S& s, int o);
	static void res_3_xix_e(S& s, int o);
	static void res_3_xix_h(S& s, int o);
	static void res_3_xix_l(S& s, int o);
	static void res_4_xix_a(S& s, int o);
	static void res_4_xix_b(S& s, int o);
	static void res_4_xix_c(S& s, int o);
	static void res_4_xix_d(S& s, int o);
	static void res_4_xix_e(S& s, int o);
	static void res_4_xix_h(S& s, int o);
	static void res_4_xix_l(S& s, int o);
	static void res_5_xix_a(S& s, int o);
	static void res_5_xix_b(S& s, int o);
	static void res_5_xix_c(S& s, int o);
	static void res_5_xix_d(S& s, int o);
	static void res_5_xix_e(S& s, int o);
	static void res_5_xix_h(S& s, int o);
	static void res_5_xix_l(S& s, int o);
	static void res_6_xix_a(S& s, int o);
	static void res_6_xix_b(S& s, int o);
	static void res_6_xix_c(S& s, int o);
	static void res_6_xix_d(S& s, int o);
	static void res_6_xix_e(S& s, int o);
	static void res_6_xix_h(S& s, int o);
	static void res_6_xix_l(S& s, int o);
	static void res_7_xix_a(S& s, int o);
	static void res_7_xix_b(S& s, int o);
	static void res_7_xix_c(S& s, int o);
	static void res_7_xix_d(S& s, int o);
	static void res_7_xix_e(S& s, int o);
	static void res_7_xix_h(S& s, int o);
	static void res_7_xix_l(S& s, int o);
	static void res_0_xiy_a(S& s, int o);
	static void res_0_xiy_b(S& s, int o);
	static void res_0_xiy_c(S& s, int o);
	static void res_0_xiy_d(S& s, int o);
	static void res_0_xiy_e(S& s, int o);
	static void res_0_xiy_h(S& s, int o);
	static void res_0_xiy_l(S& s, int o);
	static void res_1_xiy_a(S& s, int o);
	static void res_1_xiy_b(S& s, int o);
	static void res_1_xiy_c(S& s, int o);
	static void res_1_xiy_d(S& s, int o);
	static void res_1_xiy_e(S& s, int o);
	static void res_1_xiy_h(S& s, int o);
	static void res_1_xiy_l(S& s, int o);
	static void res_2_xiy_a(S& s, int o);
	static void res_2_xiy_b(S& s, int o);
	static void res_2_xiy_c(S& s, int o);
	static void res_2_xiy_d(S& s, int o);
	static void res_2_xiy_e(S& s, int o);
	static void res_2_xiy_h(S& s, int o);
	static void res_2_xiy_l(S& s, int o);
	static void res_3_xiy_a(S& s, int o);
	static void res_3_xiy_b(S& s, int o);
	static void res_3_xiy_c(S& s, int o);
	static void res_3_xiy_d(S& s, int o);
	static void res_3_xiy_e(S& s, int o);
	static void res_3_xiy_h(S& s, int o);
	static void res_3_xiy_l(S& s, int o);
	static void res_4_xiy_a(S& s, int o);
	static void res_4_xiy_b(S& s, int o);
	static void res_4_xiy_c(S& s, int o);
	static void res_4_xiy_d(S& s, int o);
	static void res_4_xiy_e(S& s, int o);
	static void res_4_xiy_h(S& s, int o);
	static void res_4_xiy_l(S& s, int o);
	static void res_5_xiy_a(S& s, int o);
	static void res_5_xiy_b(S& s, int o);
	static void res_5_xiy_c(S& s, int o);
	static void res_5_xiy_d(S& s, int o);
	static void res_5_xiy_e(S& s, int o);
	static void res_5_xiy_h(S& s, int o);
	static void res_5_xiy_l(S& s, int o);
	static void res_6_xiy_a(S& s, int o);
	static void res_6_xiy_b(S& s, int o);
	static void res_6_xiy_c(S& s, int o);
	static void res_6_xiy_d(S& s, int o);
	static void res_6_xiy_e(S& s, int o);
	static void res_6_xiy_h(S& s, int o);
	static void res_6_xiy_l(S& s, int o);
	static void res_7_xiy_a(S& s, int o);
	static void res_7_xiy_b(S& s, int o);
	static void res_7_xiy_c(S& s, int o);
	static void res_7_xiy_d(S& s, int o);
	static void res_7_xiy_e(S& s, int o);
	static void res_7_xiy_h(S& s, int o);
	static void res_7_xiy_l(S& s, int o);

	inline byte SET(unsigned bit, byte reg);
	static void set_0_a(S& s);
	static void set_0_b(S& s);
	static void set_0_c(S& s);
	static void set_0_d(S& s);
	static void set_0_e(S& s);
	static void set_0_h(S& s);
	static void set_0_l(S& s);
	static void set_1_a(S& s);
	static void set_1_b(S& s);
	static void set_1_c(S& s);
	static void set_1_d(S& s);
	static void set_1_e(S& s);
	static void set_1_h(S& s);
	static void set_1_l(S& s);
	static void set_2_a(S& s);
	static void set_2_b(S& s);
	static void set_2_c(S& s);
	static void set_2_d(S& s);
	static void set_2_e(S& s);
	static void set_2_h(S& s);
	static void set_2_l(S& s);
	static void set_3_a(S& s);
	static void set_3_b(S& s);
	static void set_3_c(S& s);
	static void set_3_d(S& s);
	static void set_3_e(S& s);
	static void set_3_h(S& s);
	static void set_3_l(S& s);
	static void set_4_a(S& s);
	static void set_4_b(S& s);
	static void set_4_c(S& s);
	static void set_4_d(S& s);
	static void set_4_e(S& s);
	static void set_4_h(S& s);
	static void set_4_l(S& s);
	static void set_5_a(S& s);
	static void set_5_b(S& s);
	static void set_5_c(S& s);
	static void set_5_d(S& s);
	static void set_5_e(S& s);
	static void set_5_h(S& s);
	static void set_5_l(S& s);
	static void set_6_a(S& s);
	static void set_6_b(S& s);
	static void set_6_c(S& s);
	static void set_6_d(S& s);
	static void set_6_e(S& s);
	static void set_6_h(S& s);
	static void set_6_l(S& s);
	static void set_7_a(S& s);
	static void set_7_b(S& s);
	static void set_7_c(S& s);
	static void set_7_d(S& s);
	static void set_7_e(S& s);
	static void set_7_h(S& s);
	static void set_7_l(S& s);

	inline byte SET_X(unsigned bit, unsigned x);
	inline byte SET_X_(unsigned bit, unsigned x);
	inline byte SET_X_X(unsigned bit, int ofst);
	inline byte SET_X_Y(unsigned bit, int ofst);
	static void set_0_xhl(S& s);
	static void set_1_xhl(S& s);
	static void set_2_xhl(S& s);
	static void set_3_xhl(S& s);
	static void set_4_xhl(S& s);
	static void set_5_xhl(S& s);
	static void set_6_xhl(S& s);
	static void set_7_xhl(S& s);
	static void set_0_xix(S& s, int o);
	static void set_1_xix(S& s, int o);
	static void set_2_xix(S& s, int o);
	static void set_3_xix(S& s, int o);
	static void set_4_xix(S& s, int o);
	static void set_5_xix(S& s, int o);
	static void set_6_xix(S& s, int o);
	static void set_7_xix(S& s, int o);
	static void set_0_xiy(S& s, int o);
	static void set_1_xiy(S& s, int o);
	static void set_2_xiy(S& s, int o);
	static void set_3_xiy(S& s, int o);
	static void set_4_xiy(S& s, int o);
	static void set_5_xiy(S& s, int o);
	static void set_6_xiy(S& s, int o);
	static void set_7_xiy(S& s, int o);
	static void set_0_xix_a(S& s, int o);
	static void set_0_xix_b(S& s, int o);
	static void set_0_xix_c(S& s, int o);
	static void set_0_xix_d(S& s, int o);
	static void set_0_xix_e(S& s, int o);
	static void set_0_xix_h(S& s, int o);
	static void set_0_xix_l(S& s, int o);
	static void set_1_xix_a(S& s, int o);
	static void set_1_xix_b(S& s, int o);
	static void set_1_xix_c(S& s, int o);
	static void set_1_xix_d(S& s, int o);
	static void set_1_xix_e(S& s, int o);
	static void set_1_xix_h(S& s, int o);
	static void set_1_xix_l(S& s, int o);
	static void set_2_xix_a(S& s, int o);
	static void set_2_xix_b(S& s, int o);
	static void set_2_xix_c(S& s, int o);
	static void set_2_xix_d(S& s, int o);
	static void set_2_xix_e(S& s, int o);
	static void set_2_xix_h(S& s, int o);
	static void set_2_xix_l(S& s, int o);
	static void set_3_xix_a(S& s, int o);
	static void set_3_xix_b(S& s, int o);
	static void set_3_xix_c(S& s, int o);
	static void set_3_xix_d(S& s, int o);
	static void set_3_xix_e(S& s, int o);
	static void set_3_xix_h(S& s, int o);
	static void set_3_xix_l(S& s, int o);
	static void set_4_xix_a(S& s, int o);
	static void set_4_xix_b(S& s, int o);
	static void set_4_xix_c(S& s, int o);
	static void set_4_xix_d(S& s, int o);
	static void set_4_xix_e(S& s, int o);
	static void set_4_xix_h(S& s, int o);
	static void set_4_xix_l(S& s, int o);
	static void set_5_xix_a(S& s, int o);
	static void set_5_xix_b(S& s, int o);
	static void set_5_xix_c(S& s, int o);
	static void set_5_xix_d(S& s, int o);
	static void set_5_xix_e(S& s, int o);
	static void set_5_xix_h(S& s, int o);
	static void set_5_xix_l(S& s, int o);
	static void set_6_xix_a(S& s, int o);
	static void set_6_xix_b(S& s, int o);
	static void set_6_xix_c(S& s, int o);
	static void set_6_xix_d(S& s, int o);
	static void set_6_xix_e(S& s, int o);
	static void set_6_xix_h(S& s, int o);
	static void set_6_xix_l(S& s, int o);
	static void set_7_xix_a(S& s, int o);
	static void set_7_xix_b(S& s, int o);
	static void set_7_xix_c(S& s, int o);
	static void set_7_xix_d(S& s, int o);
	static void set_7_xix_e(S& s, int o);
	static void set_7_xix_h(S& s, int o);
	static void set_7_xix_l(S& s, int o);
	static void set_0_xiy_a(S& s, int o);
	static void set_0_xiy_b(S& s, int o);
	static void set_0_xiy_c(S& s, int o);
	static void set_0_xiy_d(S& s, int o);
	static void set_0_xiy_e(S& s, int o);
	static void set_0_xiy_h(S& s, int o);
	static void set_0_xiy_l(S& s, int o);
	static void set_1_xiy_a(S& s, int o);
	static void set_1_xiy_b(S& s, int o);
	static void set_1_xiy_c(S& s, int o);
	static void set_1_xiy_d(S& s, int o);
	static void set_1_xiy_e(S& s, int o);
	static void set_1_xiy_h(S& s, int o);
	static void set_1_xiy_l(S& s, int o);
	static void set_2_xiy_a(S& s, int o);
	static void set_2_xiy_b(S& s, int o);
	static void set_2_xiy_c(S& s, int o);
	static void set_2_xiy_d(S& s, int o);
	static void set_2_xiy_e(S& s, int o);
	static void set_2_xiy_h(S& s, int o);
	static void set_2_xiy_l(S& s, int o);
	static void set_3_xiy_a(S& s, int o);
	static void set_3_xiy_b(S& s, int o);
	static void set_3_xiy_c(S& s, int o);
	static void set_3_xiy_d(S& s, int o);
	static void set_3_xiy_e(S& s, int o);
	static void set_3_xiy_h(S& s, int o);
	static void set_3_xiy_l(S& s, int o);
	static void set_4_xiy_a(S& s, int o);
	static void set_4_xiy_b(S& s, int o);
	static void set_4_xiy_c(S& s, int o);
	static void set_4_xiy_d(S& s, int o);
	static void set_4_xiy_e(S& s, int o);
	static void set_4_xiy_h(S& s, int o);
	static void set_4_xiy_l(S& s, int o);
	static void set_5_xiy_a(S& s, int o);
	static void set_5_xiy_b(S& s, int o);
	static void set_5_xiy_c(S& s, int o);
	static void set_5_xiy_d(S& s, int o);
	static void set_5_xiy_e(S& s, int o);
	static void set_5_xiy_h(S& s, int o);
	static void set_5_xiy_l(S& s, int o);
	static void set_6_xiy_a(S& s, int o);
	static void set_6_xiy_b(S& s, int o);
	static void set_6_xiy_c(S& s, int o);
	static void set_6_xiy_d(S& s, int o);
	static void set_6_xiy_e(S& s, int o);
	static void set_6_xiy_h(S& s, int o);
	static void set_6_xiy_l(S& s, int o);
	static void set_7_xiy_a(S& s, int o);
	static void set_7_xiy_b(S& s, int o);
	static void set_7_xiy_c(S& s, int o);
	static void set_7_xiy_d(S& s, int o);
	static void set_7_xiy_e(S& s, int o);
	static void set_7_xiy_h(S& s, int o);
	static void set_7_xiy_l(S& s, int o);

	inline byte RL(byte reg);
	inline byte RL_X(unsigned x);
	inline byte RL_X_(unsigned x);
	inline byte RL_X_X(int ofst);
	inline byte RL_X_Y(int ofst);
	static void rl_a(S& s);
	static void rl_b(S& s);
	static void rl_c(S& s);
	static void rl_d(S& s);
	static void rl_e(S& s);
	static void rl_h(S& s);
	static void rl_l(S& s);
	static void rl_xhl(S& s);
	static void rl_xix  (S& s, int o);
	static void rl_xix_a(S& s, int o);
	static void rl_xix_b(S& s, int o);
	static void rl_xix_c(S& s, int o);
	static void rl_xix_d(S& s, int o);
	static void rl_xix_e(S& s, int o);
	static void rl_xix_h(S& s, int o);
	static void rl_xix_l(S& s, int o);
	static void rl_xiy  (S& s, int o);
	static void rl_xiy_a(S& s, int o);
	static void rl_xiy_b(S& s, int o);
	static void rl_xiy_c(S& s, int o);
	static void rl_xiy_d(S& s, int o);
	static void rl_xiy_e(S& s, int o);
	static void rl_xiy_h(S& s, int o);
	static void rl_xiy_l(S& s, int o);

	inline byte RLC(byte reg);
	inline byte RLC_X(unsigned x);
	inline byte RLC_X_(unsigned x);
	inline byte RLC_X_X(int ofst);
	inline byte RLC_X_Y(int ofst);
	static void rlc_a(S& s);
	static void rlc_b(S& s);
	static void rlc_c(S& s);
	static void rlc_d(S& s);
	static void rlc_e(S& s);
	static void rlc_h(S& s);
	static void rlc_l(S& s);
	static void rlc_xhl(S& s);
	static void rlc_xix  (S& s, int o);
	static void rlc_xix_a(S& s, int o);
	static void rlc_xix_b(S& s, int o);
	static void rlc_xix_c(S& s, int o);
	static void rlc_xix_d(S& s, int o);
	static void rlc_xix_e(S& s, int o);
	static void rlc_xix_h(S& s, int o);
	static void rlc_xix_l(S& s, int o);
	static void rlc_xiy  (S& s, int o);
	static void rlc_xiy_a(S& s, int o);
	static void rlc_xiy_b(S& s, int o);
	static void rlc_xiy_c(S& s, int o);
	static void rlc_xiy_d(S& s, int o);
	static void rlc_xiy_e(S& s, int o);
	static void rlc_xiy_h(S& s, int o);
	static void rlc_xiy_l(S& s, int o);

	inline byte RR(byte reg);
	inline byte RR_X(unsigned x);
	inline byte RR_X_(unsigned x);
	inline byte RR_X_X(int ofst);
	inline byte RR_X_Y(int ofst);
	static void rr_a(S& s);
	static void rr_b(S& s);
	static void rr_c(S& s);
	static void rr_d(S& s);
	static void rr_e(S& s);
	static void rr_h(S& s);
	static void rr_l(S& s);
	static void rr_xhl(S& s);
	static void rr_xix  (S& s, int o);
	static void rr_xix_a(S& s, int o);
	static void rr_xix_b(S& s, int o);
	static void rr_xix_c(S& s, int o);
	static void rr_xix_d(S& s, int o);
	static void rr_xix_e(S& s, int o);
	static void rr_xix_h(S& s, int o);
	static void rr_xix_l(S& s, int o);
	static void rr_xiy  (S& s, int o);
	static void rr_xiy_a(S& s, int o);
	static void rr_xiy_b(S& s, int o);
	static void rr_xiy_c(S& s, int o);
	static void rr_xiy_d(S& s, int o);
	static void rr_xiy_e(S& s, int o);
	static void rr_xiy_h(S& s, int o);
	static void rr_xiy_l(S& s, int o);

	inline byte RRC(byte reg);
	inline byte RRC_X(unsigned x);
	inline byte RRC_X_(unsigned x);
	inline byte RRC_X_X(int ofst);
	inline byte RRC_X_Y(int ofst);
	static void rrc_a(S& s);
	static void rrc_b(S& s);
	static void rrc_c(S& s);
	static void rrc_d(S& s);
	static void rrc_e(S& s);
	static void rrc_h(S& s);
	static void rrc_l(S& s);
	static void rrc_xhl(S& s);
	static void rrc_xix  (S& s, int o);
	static void rrc_xix_a(S& s, int o);
	static void rrc_xix_b(S& s, int o);
	static void rrc_xix_c(S& s, int o);
	static void rrc_xix_d(S& s, int o);
	static void rrc_xix_e(S& s, int o);
	static void rrc_xix_h(S& s, int o);
	static void rrc_xix_l(S& s, int o);
	static void rrc_xiy  (S& s, int o);
	static void rrc_xiy_a(S& s, int o);
	static void rrc_xiy_b(S& s, int o);
	static void rrc_xiy_c(S& s, int o);
	static void rrc_xiy_d(S& s, int o);
	static void rrc_xiy_e(S& s, int o);
	static void rrc_xiy_h(S& s, int o);
	static void rrc_xiy_l(S& s, int o);

	inline byte SLA(byte reg);
	inline byte SLA_X(unsigned x);
	inline byte SLA_X_(unsigned x);
	inline byte SLA_X_X(int ofst);
	inline byte SLA_X_Y(int ofst);
	static void sla_a(S& s);
	static void sla_b(S& s);
	static void sla_c(S& s);
	static void sla_d(S& s);
	static void sla_e(S& s);
	static void sla_h(S& s);
	static void sla_l(S& s);
	static void sla_xhl(S& s);
	static void sla_xix  (S& s, int o);
	static void sla_xix_a(S& s, int o);
	static void sla_xix_b(S& s, int o);
	static void sla_xix_c(S& s, int o);
	static void sla_xix_d(S& s, int o);
	static void sla_xix_e(S& s, int o);
	static void sla_xix_h(S& s, int o);
	static void sla_xix_l(S& s, int o);
	static void sla_xiy  (S& s, int o);
	static void sla_xiy_a(S& s, int o);
	static void sla_xiy_b(S& s, int o);
	static void sla_xiy_c(S& s, int o);
	static void sla_xiy_d(S& s, int o);
	static void sla_xiy_e(S& s, int o);
	static void sla_xiy_h(S& s, int o);
	static void sla_xiy_l(S& s, int o);

	inline byte SLL(byte reg);
	inline byte SLL_X(unsigned x);
	inline byte SLL_X_(unsigned x);
	inline byte SLL_X_X(int ofst);
	inline byte SLL_X_Y(int ofst);
	static void sll_a(S& s);
	static void sll_b(S& s);
	static void sll_c(S& s);
	static void sll_d(S& s);
	static void sll_e(S& s);
	static void sll_h(S& s);
	static void sll_l(S& s);
	static void sll_xhl(S& s);
	static void sll_xix  (S& s, int o);
	static void sll_xix_a(S& s, int o);
	static void sll_xix_b(S& s, int o);
	static void sll_xix_c(S& s, int o);
	static void sll_xix_d(S& s, int o);
	static void sll_xix_e(S& s, int o);
	static void sll_xix_h(S& s, int o);
	static void sll_xix_l(S& s, int o);
	static void sll_xiy  (S& s, int o);
	static void sll_xiy_a(S& s, int o);
	static void sll_xiy_b(S& s, int o);
	static void sll_xiy_c(S& s, int o);
	static void sll_xiy_d(S& s, int o);
	static void sll_xiy_e(S& s, int o);
	static void sll_xiy_h(S& s, int o);
	static void sll_xiy_l(S& s, int o);

	inline byte SRA(byte reg);
	inline byte SRA_X(unsigned x);
	inline byte SRA_X_(unsigned x);
	inline byte SRA_X_X(int ofst);
	inline byte SRA_X_Y(int ofst);
	static void sra_a(S& s);
	static void sra_b(S& s);
	static void sra_c(S& s);
	static void sra_d(S& s);
	static void sra_e(S& s);
	static void sra_h(S& s);
	static void sra_l(S& s);
	static void sra_xhl(S& s);
	static void sra_xix  (S& s, int o);
	static void sra_xix_a(S& s, int o);
	static void sra_xix_b(S& s, int o);
	static void sra_xix_c(S& s, int o);
	static void sra_xix_d(S& s, int o);
	static void sra_xix_e(S& s, int o);
	static void sra_xix_h(S& s, int o);
	static void sra_xix_l(S& s, int o);
	static void sra_xiy  (S& s, int o);
	static void sra_xiy_a(S& s, int o);
	static void sra_xiy_b(S& s, int o);
	static void sra_xiy_c(S& s, int o);
	static void sra_xiy_d(S& s, int o);
	static void sra_xiy_e(S& s, int o);
	static void sra_xiy_h(S& s, int o);
	static void sra_xiy_l(S& s, int o);

	inline byte SRL(byte reg);
	inline byte SRL_X(unsigned x);
	inline byte SRL_X_(unsigned x);
	inline byte SRL_X_X(int ofst);
	inline byte SRL_X_Y(int ofst);
	static void srl_a(S& s);
	static void srl_b(S& s);
	static void srl_c(S& s);
	static void srl_d(S& s);
	static void srl_e(S& s);
	static void srl_h(S& s);
	static void srl_l(S& s);
	static void srl_xhl(S& s);
	static void srl_xix  (S& s, int o);
	static void srl_xix_a(S& s, int o);
	static void srl_xix_b(S& s, int o);
	static void srl_xix_c(S& s, int o);
	static void srl_xix_d(S& s, int o);
	static void srl_xix_e(S& s, int o);
	static void srl_xix_h(S& s, int o);
	static void srl_xix_l(S& s, int o);
	static void srl_xiy  (S& s, int o);
	static void srl_xiy_a(S& s, int o);
	static void srl_xiy_b(S& s, int o);
	static void srl_xiy_c(S& s, int o);
	static void srl_xiy_d(S& s, int o);
	static void srl_xiy_e(S& s, int o);
	static void srl_xiy_h(S& s, int o);
	static void srl_xiy_l(S& s, int o);

	static void rla(S& s);
	static void rlca(S& s);
	static void rra(S& s);
	static void rrca(S& s);

	static void rld(S& s);
	static void rrd(S& s);

	inline void PUSH(unsigned reg);
	static void push_af(S& s);
	static void push_bc(S& s);
	static void push_de(S& s);
	static void push_hl(S& s);
	static void push_ix(S& s);
	static void push_iy(S& s);

	inline unsigned POP();
	static void pop_af(S& s);
	static void pop_bc(S& s);
	static void pop_de(S& s);
	static void pop_hl(S& s);
	static void pop_ix(S& s);
	static void pop_iy(S& s);

	inline void CALL();
	inline void SKIP_JP();
	inline void SKIP_JR();
	static void call(S& s);
	static void call_c(S& s);
	static void call_m(S& s);
	static void call_nc(S& s);
	static void call_nz(S& s);
	static void call_p(S& s);
	static void call_pe(S& s);
	static void call_po(S& s);
	static void call_z(S& s);

	inline void RST(unsigned x);
	static void rst_00(S& s);
	static void rst_08(S& s);
	static void rst_10(S& s);
	static void rst_18(S& s);
	static void rst_20(S& s);
	static void rst_28(S& s);
	static void rst_30(S& s);
	static void rst_38(S& s);

	inline void RET();
	static void ret(S& s);
	static void ret_c(S& s);
	static void ret_m(S& s);
	static void ret_nc(S& s);
	static void ret_nz(S& s);
	static void ret_p(S& s);
	static void ret_pe(S& s);
	static void ret_po(S& s);
	static void ret_z(S& s);
	static void reti(S& s);
	static void retn(S& s);

	static void jp_hl(S& s);
	static void jp_ix(S& s);
	static void jp_iy(S& s);

	inline void JP();
	static void jp(S& s);
	static void jp_c(S& s);
	static void jp_m(S& s);
	static void jp_nc(S& s);
	static void jp_nz(S& s);
	static void jp_p(S& s);
	static void jp_pe(S& s);
	static void jp_po(S& s);
	static void jp_z(S& s);

	inline void JR();
	static void jr(S& s);
	static void jr_c(S& s);
	static void jr_nc(S& s);
	static void jr_nz(S& s);
	static void jr_z(S& s);
	static void djnz(S& s);

	inline unsigned EX_SP(unsigned reg);
	static void ex_xsp_hl(S& s);
	static void ex_xsp_ix(S& s);
	static void ex_xsp_iy(S& s);

	inline byte IN();
	static void in_a_c(S& s);
	static void in_b_c(S& s);
	static void in_c_c(S& s);
	static void in_d_c(S& s);
	static void in_e_c(S& s);
	static void in_h_c(S& s);
	static void in_l_c(S& s);
	static void in_0_c(S& s);

	static void in_a_byte(S& s);

	inline void OUT(byte val);
	static void out_c_a(S& s);
	static void out_c_b(S& s);
	static void out_c_c(S& s);
	static void out_c_d(S& s);
	static void out_c_e(S& s);
	static void out_c_h(S& s);
	static void out_c_l(S& s);
	static void out_c_0(S& s);

	static void out_byte_a(S& s);

	inline void BLOCK_CP(int increase, bool repeat);
	static void cpd(S& s);
	static void cpi(S& s);
	static void cpdr(S& s);
	static void cpir(S& s);

	inline void BLOCK_LD(int increase, bool repeat);
	static void ldd(S& s);
	static void ldi(S& s);
	static void lddr(S& s);
	static void ldir(S& s);

	inline void BLOCK_IN(int increase, bool repeat);
	static void ind(S& s);
	static void ini(S& s);
	static void indr(S& s);
	static void inir(S& s);

	inline void BLOCK_OUT(int increase, bool repeat);
	static void outd(S& s);
	static void outi(S& s);
	static void otdr(S& s);
	static void otir(S& s);

	static void nop(S& s);
	static void ccf(S& s);
	static void cpl(S& s);
	static void daa(S& s);
	static void neg(S& s);
	static void scf(S& s);
	static void ex_af_af(S& s);
	static void ex_de_hl(S& s);
	static void exx(S& s);
	static void di(S& s);
	static void ei(S& s);
	static void halt(S& s);
	static void im_0(S& s);
	static void im_1(S& s);
	static void im_2(S& s);

	static void ld_a_i(S& s);
	static void ld_a_r(S& s);
	static void ld_i_a(S& s);
	static void ld_r_a(S& s);

	inline void MULUB(byte reg);
	static void mulub_a_b(S& s);
	static void mulub_a_c(S& s);
	static void mulub_a_d(S& s);
	static void mulub_a_e(S& s);

	inline void MULUW(unsigned reg);
	static void muluw_hl_bc(S& s);
	static void muluw_hl_sp(S& s);

	static void dd_cb(S& s);
	static void fd_cb(S& s);
	static void cb(S& s);
	static void ed(S& s);
	static void dd(S& s);
	static void fd(S& s);

	friend class MSXCPU;
	friend class Z80TYPE;
	friend class R800TYPE;
};

} // namespace openmsx

#endif
