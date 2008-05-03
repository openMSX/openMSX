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
	byte RDMEMslow(unsigned address, unsigned cc);
	byte RDMEM_OPCODEslow(unsigned address, unsigned cc);
	void WRMEMslow(unsigned address, byte value, unsigned cc);
	inline byte RDMEM_OPCODE(unsigned cc);
	inline unsigned RD_WORD_PC(unsigned cc);
	unsigned RD_WORD_PC_slow(unsigned cc);
	inline unsigned RD_WORD(unsigned address, unsigned cc);
	unsigned RD_WORD_slow(unsigned address, unsigned cc);
	inline byte RDMEM(unsigned address, unsigned cc);
	inline void WRMEM(unsigned address, byte value, unsigned cc);
	inline void WR_WORD(unsigned address, unsigned value, unsigned cc);
	void WR_WORD_slow(unsigned address, unsigned value, unsigned cc);
	inline void WR_WORD_rev(unsigned address, unsigned value, unsigned cc);
	void WR_WORD_rev_slow(unsigned address, unsigned value, unsigned cc);

	inline void M1Cycle();
	inline void executeInstruction1(byte opcode);
	inline void nmi();
	inline void irq0();
	inline void irq1();
	inline void irq2();
	void executeFast();
	inline void executeFastInline();
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

	inline void ld_a_b();
	inline void ld_a_c();
	inline void ld_a_d();
	inline void ld_a_e();
	inline void ld_a_h();
	inline void ld_a_l();
	inline void ld_a_ixh();
	inline void ld_a_ixl();
	inline void ld_a_iyh();
	inline void ld_a_iyl();
	inline void ld_b_a();
	inline void ld_b_c();
	inline void ld_b_d();
	inline void ld_b_e();
	inline void ld_b_h();
	inline void ld_b_l();
	inline void ld_b_ixh();
	inline void ld_b_ixl();
	inline void ld_b_iyh();
	inline void ld_b_iyl();
	inline void ld_c_a();
	inline void ld_c_b();
	inline void ld_c_d();
	inline void ld_c_e();
	inline void ld_c_h();
	inline void ld_c_l();
	inline void ld_c_ixh();
	inline void ld_c_ixl();
	inline void ld_c_iyh();
	inline void ld_c_iyl();
	inline void ld_d_a();
	inline void ld_d_c();
	inline void ld_d_b();
	inline void ld_d_e();
	inline void ld_d_h();
	inline void ld_d_l();
	inline void ld_d_ixh();
	inline void ld_d_ixl();
	inline void ld_d_iyh();
	inline void ld_d_iyl();
	inline void ld_e_a();
	inline void ld_e_c();
	inline void ld_e_b();
	inline void ld_e_d();
	inline void ld_e_h();
	inline void ld_e_l();
	inline void ld_e_ixh();
	inline void ld_e_ixl();
	inline void ld_e_iyh();
	inline void ld_e_iyl();
	inline void ld_h_a();
	inline void ld_h_c();
	inline void ld_h_b();
	inline void ld_h_e();
	inline void ld_h_d();
	inline void ld_h_l();
	inline void ld_l_a();
	inline void ld_l_c();
	inline void ld_l_b();
	inline void ld_l_e();
	inline void ld_l_d();
	inline void ld_l_h();
	inline void ld_ixh_a();
	inline void ld_ixh_b();
	inline void ld_ixh_c();
	inline void ld_ixh_d();
	inline void ld_ixh_e();
	inline void ld_ixh_ixl();
	inline void ld_ixl_a();
	inline void ld_ixl_b();
	inline void ld_ixl_c();
	inline void ld_ixl_d();
	inline void ld_ixl_e();
	inline void ld_ixl_ixh();
	inline void ld_iyh_a();
	inline void ld_iyh_b();
	inline void ld_iyh_c();
	inline void ld_iyh_d();
	inline void ld_iyh_e();
	inline void ld_iyh_iyl();
	inline void ld_iyl_a();
	inline void ld_iyl_b();
	inline void ld_iyl_c();
	inline void ld_iyl_d();
	inline void ld_iyl_e();
	inline void ld_iyl_iyh();

	inline void ld_sp_hl();
	inline void ld_sp_ix();
	inline void ld_sp_iy();

	inline void WR_X_A(unsigned x);
	inline void ld_xbc_a();
	inline void ld_xde_a();
	inline void ld_xhl_a();

	inline void WR_HL_X(byte val);
	inline void ld_xhl_b();
	inline void ld_xhl_c();
	inline void ld_xhl_d();
	inline void ld_xhl_e();
	inline void ld_xhl_h();
	inline void ld_xhl_l();
	inline void ld_xhl_byte();

	inline void WR_XIX(byte val);
	inline void ld_xix_a();
	inline void ld_xix_b();
	inline void ld_xix_c();
	inline void ld_xix_d();
	inline void ld_xix_e();
	inline void ld_xix_h();
	inline void ld_xix_l();

	inline void WR_XIY(byte val);
	inline void ld_xiy_a();
	inline void ld_xiy_b();
	inline void ld_xiy_c();
	inline void ld_xiy_d();
	inline void ld_xiy_e();
	inline void ld_xiy_h();
	inline void ld_xiy_l();

	inline void ld_xix_byte();
	inline void ld_xiy_byte();

	inline void ld_xbyte_a();

	inline void WR_NN_Y(unsigned reg, int ee);
	inline void ld_xword_hl();
	inline void ld_xword_ix();
	inline void ld_xword_iy();
	inline void ld_xword_bc2();
	inline void ld_xword_de2();
	inline void ld_xword_hl2();
	inline void ld_xword_sp2();

	inline void ld_a_xbc();
	inline void ld_a_xde();
	inline void ld_a_xhl();
	inline void ld_a_xix();
	inline void ld_a_xiy();

	inline void ld_a_xbyte();

	inline void ld_a_byte();
	inline void ld_b_byte();
	inline void ld_c_byte();
	inline void ld_d_byte();
	inline void ld_e_byte();
	inline void ld_h_byte();
	inline void ld_l_byte();
	inline void ld_ixh_byte();
	inline void ld_ixl_byte();
	inline void ld_iyh_byte();
	inline void ld_iyl_byte();

	inline void ld_b_xhl();
	inline void ld_c_xhl();
	inline void ld_d_xhl();
	inline void ld_e_xhl();
	inline void ld_h_xhl();
	inline void ld_l_xhl();

	inline byte RD_R_XIX();
	inline void ld_b_xix();
	inline void ld_c_xix();
	inline void ld_d_xix();
	inline void ld_e_xix();
	inline void ld_h_xix();
	inline void ld_l_xix();

	inline byte RD_R_XIY();
	inline void ld_b_xiy();
	inline void ld_c_xiy();
	inline void ld_d_xiy();
	inline void ld_e_xiy();
	inline void ld_h_xiy();
	inline void ld_l_xiy();

	inline unsigned RD_P_XX(int ee);
	inline void ld_hl_xword();
	inline void ld_ix_xword();
	inline void ld_iy_xword();
	inline void ld_bc_xword2();
	inline void ld_de_xword2();
	inline void ld_hl_xword2();
	inline void ld_sp_xword2();

	inline void ld_bc_word();
	inline void ld_de_word();
	inline void ld_hl_word();
	inline void ld_ix_word();
	inline void ld_iy_word();
	inline void ld_sp_word();

	inline void ADC(byte reg);
	inline void adc_a_a();
	inline void adc_a_b();
	inline void adc_a_c();
	inline void adc_a_d();
	inline void adc_a_e();
	inline void adc_a_h();
	inline void adc_a_l();
	inline void adc_a_ixl();
	inline void adc_a_ixh();
	inline void adc_a_iyl();
	inline void adc_a_iyh();
	inline void adc_a_byte();
	inline void adc_a_xhl();
	inline void adc_a_xix();
	inline void adc_a_xiy();

	inline void ADD(byte reg);
	inline void add_a_a();
	inline void add_a_b();
	inline void add_a_c();
	inline void add_a_d();
	inline void add_a_e();
	inline void add_a_h();
	inline void add_a_l();
	inline void add_a_ixl();
	inline void add_a_ixh();
	inline void add_a_iyl();
	inline void add_a_iyh();
	inline void add_a_byte();
	inline void add_a_xhl();
	inline void add_a_xix();
	inline void add_a_xiy();

	inline void AND(byte reg);
	inline void and_a();
	inline void and_b();
	inline void and_c();
	inline void and_d();
	inline void and_e();
	inline void and_h();
	inline void and_l();
	inline void and_ixh();
	inline void and_ixl();
	inline void and_iyh();
	inline void and_iyl();
	inline void and_byte();
	inline void and_xhl();
	inline void and_xix();
	inline void and_xiy();

	inline void CP(byte reg);
	inline void cp_a();
	inline void cp_b();
	inline void cp_c();
	inline void cp_d();
	inline void cp_e();
	inline void cp_h();
	inline void cp_l();
	inline void cp_ixh();
	inline void cp_ixl();
	inline void cp_iyh();
	inline void cp_iyl();
	inline void cp_byte();
	inline void cp_xhl();
	inline void cp_xix();
	inline void cp_xiy();

	inline void OR(byte reg);
	inline void or_a();
	inline void or_b();
	inline void or_c();
	inline void or_d();
	inline void or_e();
	inline void or_h();
	inline void or_l();
	inline void or_ixh();
	inline void or_ixl();
	inline void or_iyh();
	inline void or_iyl();
	inline void or_byte();
	inline void or_xhl();
	inline void or_xix();
	inline void or_xiy();

	inline void SBC(byte reg);
	inline void sbc_a_a();
	inline void sbc_a_b();
	inline void sbc_a_c();
	inline void sbc_a_d();
	inline void sbc_a_e();
	inline void sbc_a_h();
	inline void sbc_a_l();
	inline void sbc_a_ixh();
	inline void sbc_a_ixl();
	inline void sbc_a_iyh();
	inline void sbc_a_iyl();
	inline void sbc_a_byte();
	inline void sbc_a_xhl();
	inline void sbc_a_xix();
	inline void sbc_a_xiy();

	inline void SUB(byte reg);
	inline void sub_a();
	inline void sub_b();
	inline void sub_c();
	inline void sub_d();
	inline void sub_e();
	inline void sub_h();
	inline void sub_l();
	inline void sub_ixh();
	inline void sub_ixl();
	inline void sub_iyh();
	inline void sub_iyl();
	inline void sub_byte();
	inline void sub_xhl();
	inline void sub_xix();
	inline void sub_xiy();

	inline void XOR(byte reg);
	inline void xor_a();
	inline void xor_b();
	inline void xor_c();
	inline void xor_d();
	inline void xor_e();
	inline void xor_h();
	inline void xor_l();
	inline void xor_ixh();
	inline void xor_ixl();
	inline void xor_iyh();
	inline void xor_iyl();
	inline void xor_byte();
	inline void xor_xhl();
	inline void xor_xix();
	inline void xor_xiy();

	inline byte DEC(byte reg);
	inline void dec_a();
	inline void dec_b();
	inline void dec_c();
	inline void dec_d();
	inline void dec_e();
	inline void dec_h();
	inline void dec_l();
	inline void dec_ixh();
	inline void dec_ixl();
	inline void dec_iyh();
	inline void dec_iyl();
	inline void DEC_X(unsigned x, int ee);
	inline void dec_xhl();
	inline void dec_xix();
	inline void dec_xiy();

	inline byte INC(byte reg);
	inline void inc_a();
	inline void inc_b();
	inline void inc_c();
	inline void inc_d();
	inline void inc_e();
	inline void inc_h();
	inline void inc_l();
	inline void inc_ixh();
	inline void inc_ixl();
	inline void inc_iyh();
	inline void inc_iyl();
	inline void INC_X(unsigned x, int ee);
	inline void inc_xhl();
	inline void inc_xix();
	inline void inc_xiy();

	inline void ADCW(unsigned reg);
	inline void adc_hl_bc();
	inline void adc_hl_de();
	inline void adc_hl_hl();
	inline void adc_hl_sp();

	inline unsigned ADDW(unsigned reg1, unsigned reg2);
	inline unsigned ADDW2(unsigned reg);
	inline void add_hl_bc();
	inline void add_hl_de();
	inline void add_hl_hl();
	inline void add_hl_sp();
	inline void add_ix_bc();
	inline void add_ix_de();
	inline void add_ix_ix();
	inline void add_ix_sp();
	inline void add_iy_bc();
	inline void add_iy_de();
	inline void add_iy_iy();
	inline void add_iy_sp();

	inline void SBCW(unsigned reg);
	inline void sbc_hl_bc();
	inline void sbc_hl_de();
	inline void sbc_hl_hl();
	inline void sbc_hl_sp();

	inline void dec_bc();
	inline void dec_de();
	inline void dec_hl();
	inline void dec_ix();
	inline void dec_iy();
	inline void dec_sp();

	inline void inc_bc();
	inline void inc_de();
	inline void inc_hl();
	inline void inc_ix();
	inline void inc_iy();
	inline void inc_sp();

	inline void BIT(byte bit, byte reg);
	inline void bit_0_a();
	inline void bit_0_b();
	inline void bit_0_c();
	inline void bit_0_d();
	inline void bit_0_e();
	inline void bit_0_h();
	inline void bit_0_l();
	inline void bit_1_a();
	inline void bit_1_b();
	inline void bit_1_c();
	inline void bit_1_d();
	inline void bit_1_e();
	inline void bit_1_h();
	inline void bit_1_l();
	inline void bit_2_a();
	inline void bit_2_b();
	inline void bit_2_c();
	inline void bit_2_d();
	inline void bit_2_e();
	inline void bit_2_h();
	inline void bit_2_l();
	inline void bit_3_a();
	inline void bit_3_b();
	inline void bit_3_c();
	inline void bit_3_d();
	inline void bit_3_e();
	inline void bit_3_h();
	inline void bit_3_l();
	inline void bit_4_a();
	inline void bit_4_b();
	inline void bit_4_c();
	inline void bit_4_d();
	inline void bit_4_e();
	inline void bit_4_h();
	inline void bit_4_l();
	inline void bit_5_a();
	inline void bit_5_b();
	inline void bit_5_c();
	inline void bit_5_d();
	inline void bit_5_e();
	inline void bit_5_h();
	inline void bit_5_l();
	inline void bit_6_a();
	inline void bit_6_b();
	inline void bit_6_c();
	inline void bit_6_d();
	inline void bit_6_e();
	inline void bit_6_h();
	inline void bit_6_l();
	inline void bit_7_a();
	inline void bit_7_b();
	inline void bit_7_c();
	inline void bit_7_d();
	inline void bit_7_e();
	inline void bit_7_h();
	inline void bit_7_l();

	inline void BIT_HL(byte bit);
	inline void bit_0_xhl();
	inline void bit_1_xhl();
	inline void bit_2_xhl();
	inline void bit_3_xhl();
	inline void bit_4_xhl();
	inline void bit_5_xhl();
	inline void bit_6_xhl();
	inline void bit_7_xhl();

	inline void BIT_IX(byte bit, int ofst);
	inline void bit_0_xix(int o);
	inline void bit_1_xix(int o);
	inline void bit_2_xix(int o);
	inline void bit_3_xix(int o);
	inline void bit_4_xix(int o);
	inline void bit_5_xix(int o);
	inline void bit_6_xix(int o);
	inline void bit_7_xix(int o);

	inline void BIT_IY(byte bit, int ofst);
	inline void bit_0_xiy(int o);
	inline void bit_1_xiy(int o);
	inline void bit_2_xiy(int o);
	inline void bit_3_xiy(int o);
	inline void bit_4_xiy(int o);
	inline void bit_5_xiy(int o);
	inline void bit_6_xiy(int o);
	inline void bit_7_xiy(int o);

	inline byte RES(unsigned bit, byte reg);
	inline void res_0_a();
	inline void res_0_b();
	inline void res_0_c();
	inline void res_0_d();
	inline void res_0_e();
	inline void res_0_h();
	inline void res_0_l();
	inline void res_1_a();
	inline void res_1_b();
	inline void res_1_c();
	inline void res_1_d();
	inline void res_1_e();
	inline void res_1_h();
	inline void res_1_l();
	inline void res_2_a();
	inline void res_2_b();
	inline void res_2_c();
	inline void res_2_d();
	inline void res_2_e();
	inline void res_2_h();
	inline void res_2_l();
	inline void res_3_a();
	inline void res_3_b();
	inline void res_3_c();
	inline void res_3_d();
	inline void res_3_e();
	inline void res_3_h();
	inline void res_3_l();
	inline void res_4_a();
	inline void res_4_b();
	inline void res_4_c();
	inline void res_4_d();
	inline void res_4_e();
	inline void res_4_h();
	inline void res_4_l();
	inline void res_5_a();
	inline void res_5_b();
	inline void res_5_c();
	inline void res_5_d();
	inline void res_5_e();
	inline void res_5_h();
	inline void res_5_l();
	inline void res_6_a();
	inline void res_6_b();
	inline void res_6_c();
	inline void res_6_d();
	inline void res_6_e();
	inline void res_6_h();
	inline void res_6_l();
	inline void res_7_a();
	inline void res_7_b();
	inline void res_7_c();
	inline void res_7_d();
	inline void res_7_e();
	inline void res_7_h();
	inline void res_7_l();

	inline byte RES_X(unsigned bit, unsigned x, int ee);
	inline byte RES_X_(unsigned bit, unsigned x);
	inline byte RES_X_X(unsigned bit, int ofst);
	inline byte RES_X_Y(unsigned bit, int ofst);
	inline void res_0_xhl();
	inline void res_1_xhl();
	inline void res_2_xhl();
	inline void res_3_xhl();
	inline void res_4_xhl();
	inline void res_5_xhl();
	inline void res_6_xhl();
	inline void res_7_xhl();
	inline void res_0_xix(int o);
	inline void res_1_xix(int o);
	inline void res_2_xix(int o);
	inline void res_3_xix(int o);
	inline void res_4_xix(int o);
	inline void res_5_xix(int o);
	inline void res_6_xix(int o);
	inline void res_7_xix(int o);
	inline void res_0_xiy(int o);
	inline void res_1_xiy(int o);
	inline void res_2_xiy(int o);
	inline void res_3_xiy(int o);
	inline void res_4_xiy(int o);
	inline void res_5_xiy(int o);
	inline void res_6_xiy(int o);
	inline void res_7_xiy(int o);
	inline void res_0_xix_a(int o);
	inline void res_0_xix_b(int o);
	inline void res_0_xix_c(int o);
	inline void res_0_xix_d(int o);
	inline void res_0_xix_e(int o);
	inline void res_0_xix_h(int o);
	inline void res_0_xix_l(int o);
	inline void res_1_xix_a(int o);
	inline void res_1_xix_b(int o);
	inline void res_1_xix_c(int o);
	inline void res_1_xix_d(int o);
	inline void res_1_xix_e(int o);
	inline void res_1_xix_h(int o);
	inline void res_1_xix_l(int o);
	inline void res_2_xix_a(int o);
	inline void res_2_xix_b(int o);
	inline void res_2_xix_c(int o);
	inline void res_2_xix_d(int o);
	inline void res_2_xix_e(int o);
	inline void res_2_xix_h(int o);
	inline void res_2_xix_l(int o);
	inline void res_3_xix_a(int o);
	inline void res_3_xix_b(int o);
	inline void res_3_xix_c(int o);
	inline void res_3_xix_d(int o);
	inline void res_3_xix_e(int o);
	inline void res_3_xix_h(int o);
	inline void res_3_xix_l(int o);
	inline void res_4_xix_a(int o);
	inline void res_4_xix_b(int o);
	inline void res_4_xix_c(int o);
	inline void res_4_xix_d(int o);
	inline void res_4_xix_e(int o);
	inline void res_4_xix_h(int o);
	inline void res_4_xix_l(int o);
	inline void res_5_xix_a(int o);
	inline void res_5_xix_b(int o);
	inline void res_5_xix_c(int o);
	inline void res_5_xix_d(int o);
	inline void res_5_xix_e(int o);
	inline void res_5_xix_h(int o);
	inline void res_5_xix_l(int o);
	inline void res_6_xix_a(int o);
	inline void res_6_xix_b(int o);
	inline void res_6_xix_c(int o);
	inline void res_6_xix_d(int o);
	inline void res_6_xix_e(int o);
	inline void res_6_xix_h(int o);
	inline void res_6_xix_l(int o);
	inline void res_7_xix_a(int o);
	inline void res_7_xix_b(int o);
	inline void res_7_xix_c(int o);
	inline void res_7_xix_d(int o);
	inline void res_7_xix_e(int o);
	inline void res_7_xix_h(int o);
	inline void res_7_xix_l(int o);
	inline void res_0_xiy_a(int o);
	inline void res_0_xiy_b(int o);
	inline void res_0_xiy_c(int o);
	inline void res_0_xiy_d(int o);
	inline void res_0_xiy_e(int o);
	inline void res_0_xiy_h(int o);
	inline void res_0_xiy_l(int o);
	inline void res_1_xiy_a(int o);
	inline void res_1_xiy_b(int o);
	inline void res_1_xiy_c(int o);
	inline void res_1_xiy_d(int o);
	inline void res_1_xiy_e(int o);
	inline void res_1_xiy_h(int o);
	inline void res_1_xiy_l(int o);
	inline void res_2_xiy_a(int o);
	inline void res_2_xiy_b(int o);
	inline void res_2_xiy_c(int o);
	inline void res_2_xiy_d(int o);
	inline void res_2_xiy_e(int o);
	inline void res_2_xiy_h(int o);
	inline void res_2_xiy_l(int o);
	inline void res_3_xiy_a(int o);
	inline void res_3_xiy_b(int o);
	inline void res_3_xiy_c(int o);
	inline void res_3_xiy_d(int o);
	inline void res_3_xiy_e(int o);
	inline void res_3_xiy_h(int o);
	inline void res_3_xiy_l(int o);
	inline void res_4_xiy_a(int o);
	inline void res_4_xiy_b(int o);
	inline void res_4_xiy_c(int o);
	inline void res_4_xiy_d(int o);
	inline void res_4_xiy_e(int o);
	inline void res_4_xiy_h(int o);
	inline void res_4_xiy_l(int o);
	inline void res_5_xiy_a(int o);
	inline void res_5_xiy_b(int o);
	inline void res_5_xiy_c(int o);
	inline void res_5_xiy_d(int o);
	inline void res_5_xiy_e(int o);
	inline void res_5_xiy_h(int o);
	inline void res_5_xiy_l(int o);
	inline void res_6_xiy_a(int o);
	inline void res_6_xiy_b(int o);
	inline void res_6_xiy_c(int o);
	inline void res_6_xiy_d(int o);
	inline void res_6_xiy_e(int o);
	inline void res_6_xiy_h(int o);
	inline void res_6_xiy_l(int o);
	inline void res_7_xiy_a(int o);
	inline void res_7_xiy_b(int o);
	inline void res_7_xiy_c(int o);
	inline void res_7_xiy_d(int o);
	inline void res_7_xiy_e(int o);
	inline void res_7_xiy_h(int o);
	inline void res_7_xiy_l(int o);

	inline byte SET(unsigned bit, byte reg);
	inline void set_0_a();
	inline void set_0_b();
	inline void set_0_c();
	inline void set_0_d();
	inline void set_0_e();
	inline void set_0_h();
	inline void set_0_l();
	inline void set_1_a();
	inline void set_1_b();
	inline void set_1_c();
	inline void set_1_d();
	inline void set_1_e();
	inline void set_1_h();
	inline void set_1_l();
	inline void set_2_a();
	inline void set_2_b();
	inline void set_2_c();
	inline void set_2_d();
	inline void set_2_e();
	inline void set_2_h();
	inline void set_2_l();
	inline void set_3_a();
	inline void set_3_b();
	inline void set_3_c();
	inline void set_3_d();
	inline void set_3_e();
	inline void set_3_h();
	inline void set_3_l();
	inline void set_4_a();
	inline void set_4_b();
	inline void set_4_c();
	inline void set_4_d();
	inline void set_4_e();
	inline void set_4_h();
	inline void set_4_l();
	inline void set_5_a();
	inline void set_5_b();
	inline void set_5_c();
	inline void set_5_d();
	inline void set_5_e();
	inline void set_5_h();
	inline void set_5_l();
	inline void set_6_a();
	inline void set_6_b();
	inline void set_6_c();
	inline void set_6_d();
	inline void set_6_e();
	inline void set_6_h();
	inline void set_6_l();
	inline void set_7_a();
	inline void set_7_b();
	inline void set_7_c();
	inline void set_7_d();
	inline void set_7_e();
	inline void set_7_h();
	inline void set_7_l();

	inline byte SET_X(unsigned bit, unsigned x, int ee);
	inline byte SET_X_(unsigned bit, unsigned x);
	inline byte SET_X_X(unsigned bit, int ofst);
	inline byte SET_X_Y(unsigned bit, int ofst);
	inline void set_0_xhl();
	inline void set_1_xhl();
	inline void set_2_xhl();
	inline void set_3_xhl();
	inline void set_4_xhl();
	inline void set_5_xhl();
	inline void set_6_xhl();
	inline void set_7_xhl();
	inline void set_0_xix(int o);
	inline void set_1_xix(int o);
	inline void set_2_xix(int o);
	inline void set_3_xix(int o);
	inline void set_4_xix(int o);
	inline void set_5_xix(int o);
	inline void set_6_xix(int o);
	inline void set_7_xix(int o);
	inline void set_0_xiy(int o);
	inline void set_1_xiy(int o);
	inline void set_2_xiy(int o);
	inline void set_3_xiy(int o);
	inline void set_4_xiy(int o);
	inline void set_5_xiy(int o);
	inline void set_6_xiy(int o);
	inline void set_7_xiy(int o);
	inline void set_0_xix_a(int o);
	inline void set_0_xix_b(int o);
	inline void set_0_xix_c(int o);
	inline void set_0_xix_d(int o);
	inline void set_0_xix_e(int o);
	inline void set_0_xix_h(int o);
	inline void set_0_xix_l(int o);
	inline void set_1_xix_a(int o);
	inline void set_1_xix_b(int o);
	inline void set_1_xix_c(int o);
	inline void set_1_xix_d(int o);
	inline void set_1_xix_e(int o);
	inline void set_1_xix_h(int o);
	inline void set_1_xix_l(int o);
	inline void set_2_xix_a(int o);
	inline void set_2_xix_b(int o);
	inline void set_2_xix_c(int o);
	inline void set_2_xix_d(int o);
	inline void set_2_xix_e(int o);
	inline void set_2_xix_h(int o);
	inline void set_2_xix_l(int o);
	inline void set_3_xix_a(int o);
	inline void set_3_xix_b(int o);
	inline void set_3_xix_c(int o);
	inline void set_3_xix_d(int o);
	inline void set_3_xix_e(int o);
	inline void set_3_xix_h(int o);
	inline void set_3_xix_l(int o);
	inline void set_4_xix_a(int o);
	inline void set_4_xix_b(int o);
	inline void set_4_xix_c(int o);
	inline void set_4_xix_d(int o);
	inline void set_4_xix_e(int o);
	inline void set_4_xix_h(int o);
	inline void set_4_xix_l(int o);
	inline void set_5_xix_a(int o);
	inline void set_5_xix_b(int o);
	inline void set_5_xix_c(int o);
	inline void set_5_xix_d(int o);
	inline void set_5_xix_e(int o);
	inline void set_5_xix_h(int o);
	inline void set_5_xix_l(int o);
	inline void set_6_xix_a(int o);
	inline void set_6_xix_b(int o);
	inline void set_6_xix_c(int o);
	inline void set_6_xix_d(int o);
	inline void set_6_xix_e(int o);
	inline void set_6_xix_h(int o);
	inline void set_6_xix_l(int o);
	inline void set_7_xix_a(int o);
	inline void set_7_xix_b(int o);
	inline void set_7_xix_c(int o);
	inline void set_7_xix_d(int o);
	inline void set_7_xix_e(int o);
	inline void set_7_xix_h(int o);
	inline void set_7_xix_l(int o);
	inline void set_0_xiy_a(int o);
	inline void set_0_xiy_b(int o);
	inline void set_0_xiy_c(int o);
	inline void set_0_xiy_d(int o);
	inline void set_0_xiy_e(int o);
	inline void set_0_xiy_h(int o);
	inline void set_0_xiy_l(int o);
	inline void set_1_xiy_a(int o);
	inline void set_1_xiy_b(int o);
	inline void set_1_xiy_c(int o);
	inline void set_1_xiy_d(int o);
	inline void set_1_xiy_e(int o);
	inline void set_1_xiy_h(int o);
	inline void set_1_xiy_l(int o);
	inline void set_2_xiy_a(int o);
	inline void set_2_xiy_b(int o);
	inline void set_2_xiy_c(int o);
	inline void set_2_xiy_d(int o);
	inline void set_2_xiy_e(int o);
	inline void set_2_xiy_h(int o);
	inline void set_2_xiy_l(int o);
	inline void set_3_xiy_a(int o);
	inline void set_3_xiy_b(int o);
	inline void set_3_xiy_c(int o);
	inline void set_3_xiy_d(int o);
	inline void set_3_xiy_e(int o);
	inline void set_3_xiy_h(int o);
	inline void set_3_xiy_l(int o);
	inline void set_4_xiy_a(int o);
	inline void set_4_xiy_b(int o);
	inline void set_4_xiy_c(int o);
	inline void set_4_xiy_d(int o);
	inline void set_4_xiy_e(int o);
	inline void set_4_xiy_h(int o);
	inline void set_4_xiy_l(int o);
	inline void set_5_xiy_a(int o);
	inline void set_5_xiy_b(int o);
	inline void set_5_xiy_c(int o);
	inline void set_5_xiy_d(int o);
	inline void set_5_xiy_e(int o);
	inline void set_5_xiy_h(int o);
	inline void set_5_xiy_l(int o);
	inline void set_6_xiy_a(int o);
	inline void set_6_xiy_b(int o);
	inline void set_6_xiy_c(int o);
	inline void set_6_xiy_d(int o);
	inline void set_6_xiy_e(int o);
	inline void set_6_xiy_h(int o);
	inline void set_6_xiy_l(int o);
	inline void set_7_xiy_a(int o);
	inline void set_7_xiy_b(int o);
	inline void set_7_xiy_c(int o);
	inline void set_7_xiy_d(int o);
	inline void set_7_xiy_e(int o);
	inline void set_7_xiy_h(int o);
	inline void set_7_xiy_l(int o);

	inline byte RL(byte reg);
	inline byte RL_X(unsigned x, int ee);
	inline byte RL_X_(unsigned x);
	inline byte RL_X_X(int ofst);
	inline byte RL_X_Y(int ofst);
	inline void rl_a();
	inline void rl_b();
	inline void rl_c();
	inline void rl_d();
	inline void rl_e();
	inline void rl_h();
	inline void rl_l();
	inline void rl_xhl();
	inline void rl_xix  (int o);
	inline void rl_xix_a(int o);
	inline void rl_xix_b(int o);
	inline void rl_xix_c(int o);
	inline void rl_xix_d(int o);
	inline void rl_xix_e(int o);
	inline void rl_xix_h(int o);
	inline void rl_xix_l(int o);
	inline void rl_xiy  (int o);
	inline void rl_xiy_a(int o);
	inline void rl_xiy_b(int o);
	inline void rl_xiy_c(int o);
	inline void rl_xiy_d(int o);
	inline void rl_xiy_e(int o);
	inline void rl_xiy_h(int o);
	inline void rl_xiy_l(int o);

	inline byte RLC(byte reg);
	inline byte RLC_X(unsigned x, int ee);
	inline byte RLC_X_(unsigned x);
	inline byte RLC_X_X(int ofst);
	inline byte RLC_X_Y(int ofst);
	inline void rlc_a();
	inline void rlc_b();
	inline void rlc_c();
	inline void rlc_d();
	inline void rlc_e();
	inline void rlc_h();
	inline void rlc_l();
	inline void rlc_xhl();
	inline void rlc_xix  (int o);
	inline void rlc_xix_a(int o);
	inline void rlc_xix_b(int o);
	inline void rlc_xix_c(int o);
	inline void rlc_xix_d(int o);
	inline void rlc_xix_e(int o);
	inline void rlc_xix_h(int o);
	inline void rlc_xix_l(int o);
	inline void rlc_xiy  (int o);
	inline void rlc_xiy_a(int o);
	inline void rlc_xiy_b(int o);
	inline void rlc_xiy_c(int o);
	inline void rlc_xiy_d(int o);
	inline void rlc_xiy_e(int o);
	inline void rlc_xiy_h(int o);
	inline void rlc_xiy_l(int o);

	inline byte RR(byte reg);
	inline byte RR_X(unsigned x, int ee);
	inline byte RR_X_(unsigned x);
	inline byte RR_X_X(int ofst);
	inline byte RR_X_Y(int ofst);
	inline void rr_a();
	inline void rr_b();
	inline void rr_c();
	inline void rr_d();
	inline void rr_e();
	inline void rr_h();
	inline void rr_l();
	inline void rr_xhl();
	inline void rr_xix  (int o);
	inline void rr_xix_a(int o);
	inline void rr_xix_b(int o);
	inline void rr_xix_c(int o);
	inline void rr_xix_d(int o);
	inline void rr_xix_e(int o);
	inline void rr_xix_h(int o);
	inline void rr_xix_l(int o);
	inline void rr_xiy  (int o);
	inline void rr_xiy_a(int o);
	inline void rr_xiy_b(int o);
	inline void rr_xiy_c(int o);
	inline void rr_xiy_d(int o);
	inline void rr_xiy_e(int o);
	inline void rr_xiy_h(int o);
	inline void rr_xiy_l(int o);

	inline byte RRC(byte reg);
	inline byte RRC_X(unsigned x, int ee);
	inline byte RRC_X_(unsigned x);
	inline byte RRC_X_X(int ofst);
	inline byte RRC_X_Y(int ofst);
	inline void rrc_a();
	inline void rrc_b();
	inline void rrc_c();
	inline void rrc_d();
	inline void rrc_e();
	inline void rrc_h();
	inline void rrc_l();
	inline void rrc_xhl();
	inline void rrc_xix  (int o);
	inline void rrc_xix_a(int o);
	inline void rrc_xix_b(int o);
	inline void rrc_xix_c(int o);
	inline void rrc_xix_d(int o);
	inline void rrc_xix_e(int o);
	inline void rrc_xix_h(int o);
	inline void rrc_xix_l(int o);
	inline void rrc_xiy  (int o);
	inline void rrc_xiy_a(int o);
	inline void rrc_xiy_b(int o);
	inline void rrc_xiy_c(int o);
	inline void rrc_xiy_d(int o);
	inline void rrc_xiy_e(int o);
	inline void rrc_xiy_h(int o);
	inline void rrc_xiy_l(int o);

	inline byte SLA(byte reg);
	inline byte SLA_X(unsigned x, int ee);
	inline byte SLA_X_(unsigned x);
	inline byte SLA_X_X(int ofst);
	inline byte SLA_X_Y(int ofst);
	inline void sla_a();
	inline void sla_b();
	inline void sla_c();
	inline void sla_d();
	inline void sla_e();
	inline void sla_h();
	inline void sla_l();
	inline void sla_xhl();
	inline void sla_xix  (int o);
	inline void sla_xix_a(int o);
	inline void sla_xix_b(int o);
	inline void sla_xix_c(int o);
	inline void sla_xix_d(int o);
	inline void sla_xix_e(int o);
	inline void sla_xix_h(int o);
	inline void sla_xix_l(int o);
	inline void sla_xiy  (int o);
	inline void sla_xiy_a(int o);
	inline void sla_xiy_b(int o);
	inline void sla_xiy_c(int o);
	inline void sla_xiy_d(int o);
	inline void sla_xiy_e(int o);
	inline void sla_xiy_h(int o);
	inline void sla_xiy_l(int o);

	inline byte SLL(byte reg);
	inline byte SLL_X(unsigned x, int ee);
	inline byte SLL_X_(unsigned x);
	inline byte SLL_X_X(int ofst);
	inline byte SLL_X_Y(int ofst);
	inline void sll_a();
	inline void sll_b();
	inline void sll_c();
	inline void sll_d();
	inline void sll_e();
	inline void sll_h();
	inline void sll_l();
	inline void sll_xhl();
	inline void sll_xix  (int o);
	inline void sll_xix_a(int o);
	inline void sll_xix_b(int o);
	inline void sll_xix_c(int o);
	inline void sll_xix_d(int o);
	inline void sll_xix_e(int o);
	inline void sll_xix_h(int o);
	inline void sll_xix_l(int o);
	inline void sll_xiy  (int o);
	inline void sll_xiy_a(int o);
	inline void sll_xiy_b(int o);
	inline void sll_xiy_c(int o);
	inline void sll_xiy_d(int o);
	inline void sll_xiy_e(int o);
	inline void sll_xiy_h(int o);
	inline void sll_xiy_l(int o);

	inline byte SRA(byte reg);
	inline byte SRA_X(unsigned x, int ee);
	inline byte SRA_X_(unsigned x);
	inline byte SRA_X_X(int ofst);
	inline byte SRA_X_Y(int ofst);
	inline void sra_a();
	inline void sra_b();
	inline void sra_c();
	inline void sra_d();
	inline void sra_e();
	inline void sra_h();
	inline void sra_l();
	inline void sra_xhl();
	inline void sra_xix  (int o);
	inline void sra_xix_a(int o);
	inline void sra_xix_b(int o);
	inline void sra_xix_c(int o);
	inline void sra_xix_d(int o);
	inline void sra_xix_e(int o);
	inline void sra_xix_h(int o);
	inline void sra_xix_l(int o);
	inline void sra_xiy  (int o);
	inline void sra_xiy_a(int o);
	inline void sra_xiy_b(int o);
	inline void sra_xiy_c(int o);
	inline void sra_xiy_d(int o);
	inline void sra_xiy_e(int o);
	inline void sra_xiy_h(int o);
	inline void sra_xiy_l(int o);

	inline byte SRL(byte reg);
	inline byte SRL_X(unsigned x, int ee);
	inline byte SRL_X_(unsigned x);
	inline byte SRL_X_X(int ofst);
	inline byte SRL_X_Y(int ofst);
	inline void srl_a();
	inline void srl_b();
	inline void srl_c();
	inline void srl_d();
	inline void srl_e();
	inline void srl_h();
	inline void srl_l();
	inline void srl_xhl();
	inline void srl_xix  (int o);
	inline void srl_xix_a(int o);
	inline void srl_xix_b(int o);
	inline void srl_xix_c(int o);
	inline void srl_xix_d(int o);
	inline void srl_xix_e(int o);
	inline void srl_xix_h(int o);
	inline void srl_xix_l(int o);
	inline void srl_xiy  (int o);
	inline void srl_xiy_a(int o);
	inline void srl_xiy_b(int o);
	inline void srl_xiy_c(int o);
	inline void srl_xiy_d(int o);
	inline void srl_xiy_e(int o);
	inline void srl_xiy_h(int o);
	inline void srl_xiy_l(int o);

	inline void rla();
	inline void rlca();
	inline void rra();
	inline void rrca();

	inline void rld();
	inline void rrd();

	inline void PUSH(unsigned reg, int ee);
	inline void push_af();
	inline void push_bc();
	inline void push_de();
	inline void push_hl();
	inline void push_ix();
	inline void push_iy();

	inline unsigned POP(int ee);
	inline void pop_af();
	inline void pop_bc();
	inline void pop_de();
	inline void pop_hl();
	inline void pop_ix();
	inline void pop_iy();

	inline void CALL();
	inline void SKIP_CALL();
	inline void call();
	inline void call_c();
	inline void call_m();
	inline void call_nc();
	inline void call_nz();
	inline void call_p();
	inline void call_pe();
	inline void call_po();
	inline void call_z();

	inline void RST(unsigned x);
	inline void rst_00();
	inline void rst_08();
	inline void rst_10();
	inline void rst_18();
	inline void rst_20();
	inline void rst_28();
	inline void rst_30();
	inline void rst_38();

	inline void RET(bool cond, int ee);
	inline void ret();
	inline void ret_c();
	inline void ret_m();
	inline void ret_nc();
	inline void ret_nz();
	inline void ret_p();
	inline void ret_pe();
	inline void ret_po();
	inline void ret_z();
	inline void reti();
	inline void retn();

	inline void jp_hl();
	inline void jp_ix();
	inline void jp_iy();

	inline void JP();
	inline void SKIP_JP();
	inline void jp();
	inline void jp_c();
	inline void jp_m();
	inline void jp_nc();
	inline void jp_nz();
	inline void jp_p();
	inline void jp_pe();
	inline void jp_po();
	inline void jp_z();

	inline void JR(int ee);
	inline void SKIP_JR(int ee);
	inline void jr();
	inline void jr_c();
	inline void jr_nc();
	inline void jr_nz();
	inline void jr_z();
	inline void djnz();

	inline unsigned EX_SP(unsigned reg);
	inline void ex_xsp_hl();
	inline void ex_xsp_ix();
	inline void ex_xsp_iy();

	inline byte IN();
	inline void in_a_c();
	inline void in_b_c();
	inline void in_c_c();
	inline void in_d_c();
	inline void in_e_c();
	inline void in_h_c();
	inline void in_l_c();
	inline void in_0_c();

	inline void in_a_byte();

	inline void OUT(byte val);
	inline void out_c_a();
	inline void out_c_b();
	inline void out_c_c();
	inline void out_c_d();
	inline void out_c_e();
	inline void out_c_h();
	inline void out_c_l();
	inline void out_c_0();

	inline void out_byte_a();

	inline void BLOCK_CP(int increase, bool repeat);
	inline void cpd();
	inline void cpi();
	inline void cpdr();
	inline void cpir();

	inline void BLOCK_LD(int increase, bool repeat);
	inline void ldd();
	inline void ldi();
	inline void lddr();
	inline void ldir();

	inline void BLOCK_IN(int increase, bool repeat);
	inline void ind();
	inline void ini();
	inline void indr();
	inline void inir();

	inline void BLOCK_OUT(int increase, bool repeat);
	inline void outd();
	inline void outi();
	inline void otdr();
	inline void otir();

	inline void nop();
	inline void ccf();
	inline void cpl();
	inline void daa();
	inline void neg();
	inline void scf();
	inline void ex_af_af();
	inline void ex_de_hl();
	inline void exx();
	inline void di();
	inline void ei();
	inline void halt();
	inline void im_0();
	inline void im_1();
	inline void im_2();

	inline void ld_a_i();
	inline void ld_a_r();
	inline void ld_i_a();
	inline void ld_r_a();

	inline void MULUB(byte reg);
	inline void mulub_a_b();
	inline void mulub_a_c();
	inline void mulub_a_d();
	inline void mulub_a_e();

	inline void MULUW(unsigned reg);
	inline void muluw_hl_bc();
	inline void muluw_hl_sp();

	inline void dd_cb();
	inline void fd_cb();
	inline void cb();
	inline void ed();
	inline void dd();
	inline void fd();

	friend class MSXCPU;
	friend class Z80TYPE;
	friend class R800TYPE;
};

} // namespace openmsx

#endif
