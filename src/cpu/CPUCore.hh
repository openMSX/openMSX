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

	inline bool C();
	inline bool NC();
	inline bool Z();
	inline bool NZ();
	inline bool M();
	inline bool P();
	inline bool PE();
	inline bool PO();

	inline int ld_a_b();
	inline int ld_a_c();
	inline int ld_a_d();
	inline int ld_a_e();
	inline int ld_a_h();
	inline int ld_a_l();
	inline int ld_a_ixh();
	inline int ld_a_ixl();
	inline int ld_a_iyh();
	inline int ld_a_iyl();
	inline int ld_b_a();
	inline int ld_b_c();
	inline int ld_b_d();
	inline int ld_b_e();
	inline int ld_b_h();
	inline int ld_b_l();
	inline int ld_b_ixh();
	inline int ld_b_ixl();
	inline int ld_b_iyh();
	inline int ld_b_iyl();
	inline int ld_c_a();
	inline int ld_c_b();
	inline int ld_c_d();
	inline int ld_c_e();
	inline int ld_c_h();
	inline int ld_c_l();
	inline int ld_c_ixh();
	inline int ld_c_ixl();
	inline int ld_c_iyh();
	inline int ld_c_iyl();
	inline int ld_d_a();
	inline int ld_d_c();
	inline int ld_d_b();
	inline int ld_d_e();
	inline int ld_d_h();
	inline int ld_d_l();
	inline int ld_d_ixh();
	inline int ld_d_ixl();
	inline int ld_d_iyh();
	inline int ld_d_iyl();
	inline int ld_e_a();
	inline int ld_e_c();
	inline int ld_e_b();
	inline int ld_e_d();
	inline int ld_e_h();
	inline int ld_e_l();
	inline int ld_e_ixh();
	inline int ld_e_ixl();
	inline int ld_e_iyh();
	inline int ld_e_iyl();
	inline int ld_h_a();
	inline int ld_h_c();
	inline int ld_h_b();
	inline int ld_h_e();
	inline int ld_h_d();
	inline int ld_h_l();
	inline int ld_l_a();
	inline int ld_l_c();
	inline int ld_l_b();
	inline int ld_l_e();
	inline int ld_l_d();
	inline int ld_l_h();
	inline int ld_ixh_a();
	inline int ld_ixh_b();
	inline int ld_ixh_c();
	inline int ld_ixh_d();
	inline int ld_ixh_e();
	inline int ld_ixh_ixl();
	inline int ld_ixl_a();
	inline int ld_ixl_b();
	inline int ld_ixl_c();
	inline int ld_ixl_d();
	inline int ld_ixl_e();
	inline int ld_ixl_ixh();
	inline int ld_iyh_a();
	inline int ld_iyh_b();
	inline int ld_iyh_c();
	inline int ld_iyh_d();
	inline int ld_iyh_e();
	inline int ld_iyh_iyl();
	inline int ld_iyl_a();
	inline int ld_iyl_b();
	inline int ld_iyl_c();
	inline int ld_iyl_d();
	inline int ld_iyl_e();
	inline int ld_iyl_iyh();

	inline int ld_sp_hl();
	inline int ld_sp_ix();
	inline int ld_sp_iy();

	inline int WR_X_A(unsigned x);
	inline int ld_xbc_a();
	inline int ld_xde_a();
	inline int ld_xhl_a();

	inline int WR_HL_X(byte val);
	inline int ld_xhl_b();
	inline int ld_xhl_c();
	inline int ld_xhl_d();
	inline int ld_xhl_e();
	inline int ld_xhl_h();
	inline int ld_xhl_l();
	inline int ld_xhl_byte();

	inline int WR_XIX(byte val);
	inline int ld_xix_a();
	inline int ld_xix_b();
	inline int ld_xix_c();
	inline int ld_xix_d();
	inline int ld_xix_e();
	inline int ld_xix_h();
	inline int ld_xix_l();

	inline int WR_XIY(byte val);
	inline int ld_xiy_a();
	inline int ld_xiy_b();
	inline int ld_xiy_c();
	inline int ld_xiy_d();
	inline int ld_xiy_e();
	inline int ld_xiy_h();
	inline int ld_xiy_l();

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
	inline int ld_a_xhl();
	inline int ld_a_xix();
	inline int ld_a_xiy();

	inline int ld_a_xbyte();

	inline int ld_a_byte();
	inline int ld_b_byte();
	inline int ld_c_byte();
	inline int ld_d_byte();
	inline int ld_e_byte();
	inline int ld_h_byte();
	inline int ld_l_byte();
	inline int ld_ixh_byte();
	inline int ld_ixl_byte();
	inline int ld_iyh_byte();
	inline int ld_iyl_byte();

	inline int ld_b_xhl();
	inline int ld_c_xhl();
	inline int ld_d_xhl();
	inline int ld_e_xhl();
	inline int ld_h_xhl();
	inline int ld_l_xhl();

	inline byte RD_R_XIX();
	inline int ld_b_xix();
	inline int ld_c_xix();
	inline int ld_d_xix();
	inline int ld_e_xix();
	inline int ld_h_xix();
	inline int ld_l_xix();

	inline byte RD_R_XIY();
	inline int ld_b_xiy();
	inline int ld_c_xiy();
	inline int ld_d_xiy();
	inline int ld_e_xiy();
	inline int ld_h_xiy();
	inline int ld_l_xiy();

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
	inline int adc_a_b();
	inline int adc_a_c();
	inline int adc_a_d();
	inline int adc_a_e();
	inline int adc_a_h();
	inline int adc_a_l();
	inline int adc_a_ixl();
	inline int adc_a_ixh();
	inline int adc_a_iyl();
	inline int adc_a_iyh();
	inline int adc_a_byte();
	inline int adc_a_xhl();
	inline int adc_a_xix();
	inline int adc_a_xiy();

	inline void ADD(byte reg);
	inline int add_a_a();
	inline int add_a_b();
	inline int add_a_c();
	inline int add_a_d();
	inline int add_a_e();
	inline int add_a_h();
	inline int add_a_l();
	inline int add_a_ixl();
	inline int add_a_ixh();
	inline int add_a_iyl();
	inline int add_a_iyh();
	inline int add_a_byte();
	inline int add_a_xhl();
	inline int add_a_xix();
	inline int add_a_xiy();

	inline void AND(byte reg);
	inline int and_a();
	inline int and_b();
	inline int and_c();
	inline int and_d();
	inline int and_e();
	inline int and_h();
	inline int and_l();
	inline int and_ixh();
	inline int and_ixl();
	inline int and_iyh();
	inline int and_iyl();
	inline int and_byte();
	inline int and_xhl();
	inline int and_xix();
	inline int and_xiy();

	inline void CP(byte reg);
	inline int cp_a();
	inline int cp_b();
	inline int cp_c();
	inline int cp_d();
	inline int cp_e();
	inline int cp_h();
	inline int cp_l();
	inline int cp_ixh();
	inline int cp_ixl();
	inline int cp_iyh();
	inline int cp_iyl();
	inline int cp_byte();
	inline int cp_xhl();
	inline int cp_xix();
	inline int cp_xiy();

	inline void OR(byte reg);
	inline int or_a();
	inline int or_b();
	inline int or_c();
	inline int or_d();
	inline int or_e();
	inline int or_h();
	inline int or_l();
	inline int or_ixh();
	inline int or_ixl();
	inline int or_iyh();
	inline int or_iyl();
	inline int or_byte();
	inline int or_xhl();
	inline int or_xix();
	inline int or_xiy();

	inline void SBC(byte reg);
	inline int sbc_a_a();
	inline int sbc_a_b();
	inline int sbc_a_c();
	inline int sbc_a_d();
	inline int sbc_a_e();
	inline int sbc_a_h();
	inline int sbc_a_l();
	inline int sbc_a_ixh();
	inline int sbc_a_ixl();
	inline int sbc_a_iyh();
	inline int sbc_a_iyl();
	inline int sbc_a_byte();
	inline int sbc_a_xhl();
	inline int sbc_a_xix();
	inline int sbc_a_xiy();

	inline void SUB(byte reg);
	inline int sub_a();
	inline int sub_b();
	inline int sub_c();
	inline int sub_d();
	inline int sub_e();
	inline int sub_h();
	inline int sub_l();
	inline int sub_ixh();
	inline int sub_ixl();
	inline int sub_iyh();
	inline int sub_iyl();
	inline int sub_byte();
	inline int sub_xhl();
	inline int sub_xix();
	inline int sub_xiy();

	inline void XOR(byte reg);
	inline int xor_a();
	inline int xor_b();
	inline int xor_c();
	inline int xor_d();
	inline int xor_e();
	inline int xor_h();
	inline int xor_l();
	inline int xor_ixh();
	inline int xor_ixl();
	inline int xor_iyh();
	inline int xor_iyl();
	inline int xor_byte();
	inline int xor_xhl();
	inline int xor_xix();
	inline int xor_xiy();

	inline byte DEC(byte reg);
	inline int dec_a();
	inline int dec_b();
	inline int dec_c();
	inline int dec_d();
	inline int dec_e();
	inline int dec_h();
	inline int dec_l();
	inline int dec_ixh();
	inline int dec_ixl();
	inline int dec_iyh();
	inline int dec_iyl();
	inline int DEC_X(unsigned x, int ee);
	inline int dec_xhl();
	inline int dec_xix();
	inline int dec_xiy();

	inline byte INC(byte reg);
	inline int inc_a();
	inline int inc_b();
	inline int inc_c();
	inline int inc_d();
	inline int inc_e();
	inline int inc_h();
	inline int inc_l();
	inline int inc_ixh();
	inline int inc_ixl();
	inline int inc_iyh();
	inline int inc_iyl();
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

	inline void BIT(byte bit, byte reg);
	inline int bit_0_a();
	inline int bit_0_b();
	inline int bit_0_c();
	inline int bit_0_d();
	inline int bit_0_e();
	inline int bit_0_h();
	inline int bit_0_l();
	inline int bit_1_a();
	inline int bit_1_b();
	inline int bit_1_c();
	inline int bit_1_d();
	inline int bit_1_e();
	inline int bit_1_h();
	inline int bit_1_l();
	inline int bit_2_a();
	inline int bit_2_b();
	inline int bit_2_c();
	inline int bit_2_d();
	inline int bit_2_e();
	inline int bit_2_h();
	inline int bit_2_l();
	inline int bit_3_a();
	inline int bit_3_b();
	inline int bit_3_c();
	inline int bit_3_d();
	inline int bit_3_e();
	inline int bit_3_h();
	inline int bit_3_l();
	inline int bit_4_a();
	inline int bit_4_b();
	inline int bit_4_c();
	inline int bit_4_d();
	inline int bit_4_e();
	inline int bit_4_h();
	inline int bit_4_l();
	inline int bit_5_a();
	inline int bit_5_b();
	inline int bit_5_c();
	inline int bit_5_d();
	inline int bit_5_e();
	inline int bit_5_h();
	inline int bit_5_l();
	inline int bit_6_a();
	inline int bit_6_b();
	inline int bit_6_c();
	inline int bit_6_d();
	inline int bit_6_e();
	inline int bit_6_h();
	inline int bit_6_l();
	inline int bit_7_a();
	inline int bit_7_b();
	inline int bit_7_c();
	inline int bit_7_d();
	inline int bit_7_e();
	inline int bit_7_h();
	inline int bit_7_l();

	inline int BIT_HL(byte bit);
	inline int bit_0_xhl();
	inline int bit_1_xhl();
	inline int bit_2_xhl();
	inline int bit_3_xhl();
	inline int bit_4_xhl();
	inline int bit_5_xhl();
	inline int bit_6_xhl();
	inline int bit_7_xhl();

	inline int BIT_IX(byte bit, unsigned addr);
	inline int bit_0_xix(unsigned a);
	inline int bit_1_xix(unsigned a);
	inline int bit_2_xix(unsigned a);
	inline int bit_3_xix(unsigned a);
	inline int bit_4_xix(unsigned a);
	inline int bit_5_xix(unsigned a);
	inline int bit_6_xix(unsigned a);
	inline int bit_7_xix(unsigned a);

	inline byte RES(unsigned bit, byte reg);
	inline int res_0_a();
	inline int res_0_b();
	inline int res_0_c();
	inline int res_0_d();
	inline int res_0_e();
	inline int res_0_h();
	inline int res_0_l();
	inline int res_1_a();
	inline int res_1_b();
	inline int res_1_c();
	inline int res_1_d();
	inline int res_1_e();
	inline int res_1_h();
	inline int res_1_l();
	inline int res_2_a();
	inline int res_2_b();
	inline int res_2_c();
	inline int res_2_d();
	inline int res_2_e();
	inline int res_2_h();
	inline int res_2_l();
	inline int res_3_a();
	inline int res_3_b();
	inline int res_3_c();
	inline int res_3_d();
	inline int res_3_e();
	inline int res_3_h();
	inline int res_3_l();
	inline int res_4_a();
	inline int res_4_b();
	inline int res_4_c();
	inline int res_4_d();
	inline int res_4_e();
	inline int res_4_h();
	inline int res_4_l();
	inline int res_5_a();
	inline int res_5_b();
	inline int res_5_c();
	inline int res_5_d();
	inline int res_5_e();
	inline int res_5_h();
	inline int res_5_l();
	inline int res_6_a();
	inline int res_6_b();
	inline int res_6_c();
	inline int res_6_d();
	inline int res_6_e();
	inline int res_6_h();
	inline int res_6_l();
	inline int res_7_a();
	inline int res_7_b();
	inline int res_7_c();
	inline int res_7_d();
	inline int res_7_e();
	inline int res_7_h();
	inline int res_7_l();

	inline byte RES_X(unsigned bit, unsigned addr, int ee);
	inline byte RES_X_(unsigned bit, unsigned addr);
	inline int res_0_xhl();
	inline int res_1_xhl();
	inline int res_2_xhl();
	inline int res_3_xhl();
	inline int res_4_xhl();
	inline int res_5_xhl();
	inline int res_6_xhl();
	inline int res_7_xhl();
	inline int res_0_xix(unsigned a);
	inline int res_1_xix(unsigned a);
	inline int res_2_xix(unsigned a);
	inline int res_3_xix(unsigned a);
	inline int res_4_xix(unsigned a);
	inline int res_5_xix(unsigned a);
	inline int res_6_xix(unsigned a);
	inline int res_7_xix(unsigned a);
	inline int res_0_xix_a(unsigned a);
	inline int res_0_xix_b(unsigned a);
	inline int res_0_xix_c(unsigned a);
	inline int res_0_xix_d(unsigned a);
	inline int res_0_xix_e(unsigned a);
	inline int res_0_xix_h(unsigned a);
	inline int res_0_xix_l(unsigned a);
	inline int res_1_xix_a(unsigned a);
	inline int res_1_xix_b(unsigned a);
	inline int res_1_xix_c(unsigned a);
	inline int res_1_xix_d(unsigned a);
	inline int res_1_xix_e(unsigned a);
	inline int res_1_xix_h(unsigned a);
	inline int res_1_xix_l(unsigned a);
	inline int res_2_xix_a(unsigned a);
	inline int res_2_xix_b(unsigned a);
	inline int res_2_xix_c(unsigned a);
	inline int res_2_xix_d(unsigned a);
	inline int res_2_xix_e(unsigned a);
	inline int res_2_xix_h(unsigned a);
	inline int res_2_xix_l(unsigned a);
	inline int res_3_xix_a(unsigned a);
	inline int res_3_xix_b(unsigned a);
	inline int res_3_xix_c(unsigned a);
	inline int res_3_xix_d(unsigned a);
	inline int res_3_xix_e(unsigned a);
	inline int res_3_xix_h(unsigned a);
	inline int res_3_xix_l(unsigned a);
	inline int res_4_xix_a(unsigned a);
	inline int res_4_xix_b(unsigned a);
	inline int res_4_xix_c(unsigned a);
	inline int res_4_xix_d(unsigned a);
	inline int res_4_xix_e(unsigned a);
	inline int res_4_xix_h(unsigned a);
	inline int res_4_xix_l(unsigned a);
	inline int res_5_xix_a(unsigned a);
	inline int res_5_xix_b(unsigned a);
	inline int res_5_xix_c(unsigned a);
	inline int res_5_xix_d(unsigned a);
	inline int res_5_xix_e(unsigned a);
	inline int res_5_xix_h(unsigned a);
	inline int res_5_xix_l(unsigned a);
	inline int res_6_xix_a(unsigned a);
	inline int res_6_xix_b(unsigned a);
	inline int res_6_xix_c(unsigned a);
	inline int res_6_xix_d(unsigned a);
	inline int res_6_xix_e(unsigned a);
	inline int res_6_xix_h(unsigned a);
	inline int res_6_xix_l(unsigned a);
	inline int res_7_xix_a(unsigned a);
	inline int res_7_xix_b(unsigned a);
	inline int res_7_xix_c(unsigned a);
	inline int res_7_xix_d(unsigned a);
	inline int res_7_xix_e(unsigned a);
	inline int res_7_xix_h(unsigned a);
	inline int res_7_xix_l(unsigned a);

	inline byte SET(unsigned bit, byte reg);
	inline int set_0_a();
	inline int set_0_b();
	inline int set_0_c();
	inline int set_0_d();
	inline int set_0_e();
	inline int set_0_h();
	inline int set_0_l();
	inline int set_1_a();
	inline int set_1_b();
	inline int set_1_c();
	inline int set_1_d();
	inline int set_1_e();
	inline int set_1_h();
	inline int set_1_l();
	inline int set_2_a();
	inline int set_2_b();
	inline int set_2_c();
	inline int set_2_d();
	inline int set_2_e();
	inline int set_2_h();
	inline int set_2_l();
	inline int set_3_a();
	inline int set_3_b();
	inline int set_3_c();
	inline int set_3_d();
	inline int set_3_e();
	inline int set_3_h();
	inline int set_3_l();
	inline int set_4_a();
	inline int set_4_b();
	inline int set_4_c();
	inline int set_4_d();
	inline int set_4_e();
	inline int set_4_h();
	inline int set_4_l();
	inline int set_5_a();
	inline int set_5_b();
	inline int set_5_c();
	inline int set_5_d();
	inline int set_5_e();
	inline int set_5_h();
	inline int set_5_l();
	inline int set_6_a();
	inline int set_6_b();
	inline int set_6_c();
	inline int set_6_d();
	inline int set_6_e();
	inline int set_6_h();
	inline int set_6_l();
	inline int set_7_a();
	inline int set_7_b();
	inline int set_7_c();
	inline int set_7_d();
	inline int set_7_e();
	inline int set_7_h();
	inline int set_7_l();

	inline byte SET_X(unsigned bit, unsigned addr, int ee);
	inline byte SET_X_(unsigned bit, unsigned addr);
	inline int set_0_xhl();
	inline int set_1_xhl();
	inline int set_2_xhl();
	inline int set_3_xhl();
	inline int set_4_xhl();
	inline int set_5_xhl();
	inline int set_6_xhl();
	inline int set_7_xhl();
	inline int set_0_xix(unsigned a);
	inline int set_1_xix(unsigned a);
	inline int set_2_xix(unsigned a);
	inline int set_3_xix(unsigned a);
	inline int set_4_xix(unsigned a);
	inline int set_5_xix(unsigned a);
	inline int set_6_xix(unsigned a);
	inline int set_7_xix(unsigned a);
	inline int set_0_xix_a(unsigned a);
	inline int set_0_xix_b(unsigned a);
	inline int set_0_xix_c(unsigned a);
	inline int set_0_xix_d(unsigned a);
	inline int set_0_xix_e(unsigned a);
	inline int set_0_xix_h(unsigned a);
	inline int set_0_xix_l(unsigned a);
	inline int set_1_xix_a(unsigned a);
	inline int set_1_xix_b(unsigned a);
	inline int set_1_xix_c(unsigned a);
	inline int set_1_xix_d(unsigned a);
	inline int set_1_xix_e(unsigned a);
	inline int set_1_xix_h(unsigned a);
	inline int set_1_xix_l(unsigned a);
	inline int set_2_xix_a(unsigned a);
	inline int set_2_xix_b(unsigned a);
	inline int set_2_xix_c(unsigned a);
	inline int set_2_xix_d(unsigned a);
	inline int set_2_xix_e(unsigned a);
	inline int set_2_xix_h(unsigned a);
	inline int set_2_xix_l(unsigned a);
	inline int set_3_xix_a(unsigned a);
	inline int set_3_xix_b(unsigned a);
	inline int set_3_xix_c(unsigned a);
	inline int set_3_xix_d(unsigned a);
	inline int set_3_xix_e(unsigned a);
	inline int set_3_xix_h(unsigned a);
	inline int set_3_xix_l(unsigned a);
	inline int set_4_xix_a(unsigned a);
	inline int set_4_xix_b(unsigned a);
	inline int set_4_xix_c(unsigned a);
	inline int set_4_xix_d(unsigned a);
	inline int set_4_xix_e(unsigned a);
	inline int set_4_xix_h(unsigned a);
	inline int set_4_xix_l(unsigned a);
	inline int set_5_xix_a(unsigned a);
	inline int set_5_xix_b(unsigned a);
	inline int set_5_xix_c(unsigned a);
	inline int set_5_xix_d(unsigned a);
	inline int set_5_xix_e(unsigned a);
	inline int set_5_xix_h(unsigned a);
	inline int set_5_xix_l(unsigned a);
	inline int set_6_xix_a(unsigned a);
	inline int set_6_xix_b(unsigned a);
	inline int set_6_xix_c(unsigned a);
	inline int set_6_xix_d(unsigned a);
	inline int set_6_xix_e(unsigned a);
	inline int set_6_xix_h(unsigned a);
	inline int set_6_xix_l(unsigned a);
	inline int set_7_xix_a(unsigned a);
	inline int set_7_xix_b(unsigned a);
	inline int set_7_xix_c(unsigned a);
	inline int set_7_xix_d(unsigned a);
	inline int set_7_xix_e(unsigned a);
	inline int set_7_xix_h(unsigned a);
	inline int set_7_xix_l(unsigned a);

	inline byte RL(byte reg);
	inline byte RL_X(unsigned x, int ee);
	inline byte RL_X_(unsigned x);
	inline int rl_a();
	inline int rl_b();
	inline int rl_c();
	inline int rl_d();
	inline int rl_e();
	inline int rl_h();
	inline int rl_l();
	inline int rl_xhl();
	inline int rl_xix  (unsigned a);
	inline int rl_xix_a(unsigned a);
	inline int rl_xix_b(unsigned a);
	inline int rl_xix_c(unsigned a);
	inline int rl_xix_d(unsigned a);
	inline int rl_xix_e(unsigned a);
	inline int rl_xix_h(unsigned a);
	inline int rl_xix_l(unsigned a);

	inline byte RLC(byte reg);
	inline byte RLC_X(unsigned x, int ee);
	inline byte RLC_X_(unsigned x);
	inline int rlc_a();
	inline int rlc_b();
	inline int rlc_c();
	inline int rlc_d();
	inline int rlc_e();
	inline int rlc_h();
	inline int rlc_l();
	inline int rlc_xhl();
	inline int rlc_xix  (unsigned a);
	inline int rlc_xix_a(unsigned a);
	inline int rlc_xix_b(unsigned a);
	inline int rlc_xix_c(unsigned a);
	inline int rlc_xix_d(unsigned a);
	inline int rlc_xix_e(unsigned a);
	inline int rlc_xix_h(unsigned a);
	inline int rlc_xix_l(unsigned a);

	inline byte RR(byte reg);
	inline byte RR_X(unsigned x, int ee);
	inline byte RR_X_(unsigned x);
	inline int rr_a();
	inline int rr_b();
	inline int rr_c();
	inline int rr_d();
	inline int rr_e();
	inline int rr_h();
	inline int rr_l();
	inline int rr_xhl();
	inline int rr_xix  (unsigned a);
	inline int rr_xix_a(unsigned a);
	inline int rr_xix_b(unsigned a);
	inline int rr_xix_c(unsigned a);
	inline int rr_xix_d(unsigned a);
	inline int rr_xix_e(unsigned a);
	inline int rr_xix_h(unsigned a);
	inline int rr_xix_l(unsigned a);

	inline byte RRC(byte reg);
	inline byte RRC_X(unsigned x, int ee);
	inline byte RRC_X_(unsigned x);
	inline int rrc_a();
	inline int rrc_b();
	inline int rrc_c();
	inline int rrc_d();
	inline int rrc_e();
	inline int rrc_h();
	inline int rrc_l();
	inline int rrc_xhl();
	inline int rrc_xix  (unsigned a);
	inline int rrc_xix_a(unsigned a);
	inline int rrc_xix_b(unsigned a);
	inline int rrc_xix_c(unsigned a);
	inline int rrc_xix_d(unsigned a);
	inline int rrc_xix_e(unsigned a);
	inline int rrc_xix_h(unsigned a);
	inline int rrc_xix_l(unsigned a);

	inline byte SLA(byte reg);
	inline byte SLA_X(unsigned x, int ee);
	inline byte SLA_X_(unsigned x);
	inline int sla_a();
	inline int sla_b();
	inline int sla_c();
	inline int sla_d();
	inline int sla_e();
	inline int sla_h();
	inline int sla_l();
	inline int sla_xhl();
	inline int sla_xix  (unsigned a);
	inline int sla_xix_a(unsigned a);
	inline int sla_xix_b(unsigned a);
	inline int sla_xix_c(unsigned a);
	inline int sla_xix_d(unsigned a);
	inline int sla_xix_e(unsigned a);
	inline int sla_xix_h(unsigned a);
	inline int sla_xix_l(unsigned a);

	inline byte SLL(byte reg);
	inline byte SLL_X(unsigned x, int ee);
	inline byte SLL_X_(unsigned x);
	inline int sll_a();
	inline int sll_b();
	inline int sll_c();
	inline int sll_d();
	inline int sll_e();
	inline int sll_h();
	inline int sll_l();
	inline int sll_xhl();
	inline int sll_xix  (unsigned a);
	inline int sll_xix_a(unsigned a);
	inline int sll_xix_b(unsigned a);
	inline int sll_xix_c(unsigned a);
	inline int sll_xix_d(unsigned a);
	inline int sll_xix_e(unsigned a);
	inline int sll_xix_h(unsigned a);
	inline int sll_xix_l(unsigned a);

	inline byte SRA(byte reg);
	inline byte SRA_X(unsigned x, int ee);
	inline byte SRA_X_(unsigned x);
	inline int sra_a();
	inline int sra_b();
	inline int sra_c();
	inline int sra_d();
	inline int sra_e();
	inline int sra_h();
	inline int sra_l();
	inline int sra_xhl();
	inline int sra_xix  (unsigned a);
	inline int sra_xix_a(unsigned a);
	inline int sra_xix_b(unsigned a);
	inline int sra_xix_c(unsigned a);
	inline int sra_xix_d(unsigned a);
	inline int sra_xix_e(unsigned a);
	inline int sra_xix_h(unsigned a);
	inline int sra_xix_l(unsigned a);

	inline byte SRL(byte reg);
	inline byte SRL_X(unsigned x, int ee);
	inline byte SRL_X_(unsigned x);
	inline int srl_a();
	inline int srl_b();
	inline int srl_c();
	inline int srl_d();
	inline int srl_e();
	inline int srl_h();
	inline int srl_l();
	inline int srl_xhl();
	inline int srl_xix  (unsigned a);
	inline int srl_xix_a(unsigned a);
	inline int srl_xix_b(unsigned a);
	inline int srl_xix_c(unsigned a);
	inline int srl_xix_d(unsigned a);
	inline int srl_xix_e(unsigned a);
	inline int srl_xix_h(unsigned a);
	inline int srl_xix_l(unsigned a);

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

	inline int RST(unsigned x);
	inline int rst_00();
	inline int rst_08();
	inline int rst_10();
	inline int rst_18();
	inline int rst_20();
	inline int rst_28();
	inline int rst_30();
	inline int rst_38();

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

	inline byte IN();
	inline int in_a_c();
	inline int in_b_c();
	inline int in_c_c();
	inline int in_d_c();
	inline int in_e_c();
	inline int in_h_c();
	inline int in_l_c();
	inline int in_0_c();

	inline int in_a_byte();

	inline int OUT(byte val);
	inline int out_c_a();
	inline int out_c_b();
	inline int out_c_c();
	inline int out_c_d();
	inline int out_c_e();
	inline int out_c_h();
	inline int out_c_l();
	inline int out_c_0();

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
	inline int im_0();
	inline int im_1();
	inline int im_2();

	inline int ld_a_i();
	inline int ld_a_r();
	inline int ld_i_a();
	inline int ld_r_a();

	inline int MULUB(byte reg);
	inline int mulub_a_b();
	inline int mulub_a_c();
	inline int mulub_a_d();
	inline int mulub_a_e();

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
