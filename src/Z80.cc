// $Id$

/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.c                                ***/
/***                                                                      ***/
/*** This file contains the emulation code                                ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include "Z80.hh"
#include "Z80Tables.hh"

#ifdef DEBUG
#include "Z80Dasm.h"
#endif

Z80::Z80(Z80Interface *interf)
{
	interface = interf;
}
Z80::~Z80()
{
}

/****************************************************************************/
/* Initialise the various lookup tables used by the emulation code          */
/****************************************************************************/
void Z80::init ()
{
	for (int i=0; i<256; ++i) {
		byte zs = 0;
		if (i==0)   zs |= Z_FLAG;
		if (i&0x80) zs |= S_FLAG;
		int p=0;
		if (i&1)   ++p;
		if (i&2)   ++p;
		if (i&4)   ++p;
		if (i&8)   ++p;
		if (i&16)  ++p;
		if (i&32)  ++p;
		if (i&64)  ++p;
		if (i&128) ++p;
		PTable  [i] = (p&1)? 0:V_FLAG;
		ZSTable [i] = zs;
		ZSPTable[i] = zs|PTable[i];
	}
	for (int i=0; i<256; ++i) {
		ZSTable [i+256] = ZSTable [i]|C_FLAG;
		ZSPTable[i+256] = ZSPTable[i]|C_FLAG;
		PTable  [i+256] = PTable  [i]|C_FLAG;
	}
}

/****************************************************************************/
/* Reset the CPU emulation core                                             */
/* Set registers to their initial values                                    */
/* and make the Z80 the starting CPU                                        */
/****************************************************************************/
void Z80::reset()
{
	//Clear all data
	memset (&R,0,sizeof(CPU_Regs));
	R.SP.W.l = 0xF000;
	R.IX.W.l = 0xFFFF;
	R.IY.W.l = 0xFFFF;
	R.R = 0; //R.R = (byte)rand();
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/
int Z80::Z80_SingleInstruction() 
{
	byte opcode = 0;	// prevent warning
	if (interface->NMIStatus()) {
		R.HALT = false; 
		R.IFF1 = R.nextIFF1 = false;
		M_PUSH (R.PC.W.l);
		R.PC.W.l=0x0066;
		return 10;	//TODO this value is wrong
	} else if (R.IFF1 && interface->IRQStatus()) {
		R.HALT = false; 
		R.IFF1 = R.nextIFF1 = false;
		switch (R.IM) {
		case 2:
			// Interrupt mode 2. Call [R.I:databyte]
			M_PUSH (R.PC.W.l);
			R.PC.W.l=Z80_RDMEM_WORD((interface->dataBus())|(R.I<<8));
			return 10;	//TODO this value is wrong
		case 1:
			// Interrupt mode 1. RST 38h
			opcode = 0xff;
			break;
		case 0:
			// Interrupt mode 0.
			// TODO current implementation only works for 1-byte instructions
			//      ok for MSX 
			opcode = interface->dataBus();
			break;
		default:
			assert(false);
		}
	} else if (R.HALT) {
		opcode = 0;	// nop
	} else {
		opcode = Z80_RDOP(R.PC.W.l++);
	}
	R.IFF1 = R.nextIFF1;
	#ifdef DEBUG
		word start_pc = R.PC.W.l;
	#endif
	++R.R;
	R.ICount = cycles_main[opcode];
	(this->*opcode_main[opcode])();;	// R.ICount can be raised extra
	#ifdef DEBUG
		printf("%04x : instruction ", start_pc);
		Z80_Dasm(&debugmemory[start_pc], to_print_string, start_pc );
		printf("%s\n", to_print_string );
		printf("      A=%02x F=%02x \n", R.AF.B.h, R.AF.B.l);
		printf("      BC=%04x DE=%04x HL=%04x \n", R.BC.W.l, R.DE.W.l, R.HL.W.l);
	#endif
	return R.ICount;
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80::Z80_SetWaitStates (int n)
{
	for (int i=0; i<256; ++i) {
		cycles_main[i] += n;
		cycles_cb[i]   += n;
		cycles_ed[i]   += n;
		cycles_xx[i]   += n;
	}
}



/*
 * Input a byte from given I/O port
 */
inline byte Z80::Z80_In (word port) {
	return interface->readIO(port);
}
/*
 * Output a byte to given I/O port
 */
inline void Z80::Z80_Out (word port,byte value) { 
	interface->writeIO(port, value);
}
/*
 * Read a byte from given memory location
 */
inline byte Z80::Z80_RDMEM(word address) { 
#ifdef DEBUG
	debugmemory[address] = interface->readMem(address);
	return debugmemory[address];
#else
	return interface->readMem(address);
#endif
}
/*
 * Write a byte to given memory location
 */
inline void Z80::Z80_WRMEM(word address, byte value) {
	interface->writeMem(address, value);
	// No debugmemory[A] here otherwise self-modifying code could
	// alter the executing code before the disassembled opcode
	// is printed;
}

/*
 * Z80_RDOP() is identical to Z80_RDMEM() except it is used for reading
 * opcodes. In case of system with memory mapped I/O, this function can be
 * used to greatly speed up emulation
 */
inline byte Z80::Z80_RDOP(word A) { return Z80_RDMEM(A); }
/*
 * Z80_RDOP_ARG() is identical to Z80_RDOP() except it is used for reading
 * opcode arguments. This difference can be used to support systems that
 * use different encoding mechanisms for opcodes and opcode arguments
 */
inline byte Z80::Z80_RDOP_ARG(word A) { return Z80_RDOP(A); }
/*
 * Z80_RDSTACK() is identical to Z80_RDMEM() except it is used for reading
 * stack variables. In case of system with memory mapped I/O, this function
 * can be used to slightly speed up emulation
 */
inline byte Z80::Z80_RDSTACK(word A) { return Z80_RDMEM(A); }
/*
 * Z80_WRSTACK() is identical to Z80_WRMEM() except it is used for writing
 * stack variables. In case of system with memory mapped I/O, this function
 * can be used to slightly speed up emulation
 */
inline void Z80::Z80_WRSTACK(word A, byte V) { Z80_WRMEM(A,V); }


inline void Z80::M_SKIP_CALL() { R.PC.W.l += 2; }
inline void Z80::M_SKIP_JP()   { R.PC.W.l += 2; }
inline void Z80::M_SKIP_JR()   { R.PC.W.l += 1; }
inline void Z80::M_SKIP_RET()  { }

inline bool Z80::M_C()  { return R.AF.B.l&C_FLAG; }
inline bool Z80::M_NC() { return !M_C(); }
inline bool Z80::M_Z()  { return R.AF.B.l&Z_FLAG; }
inline bool Z80::M_NZ() { return !M_Z(); }
inline bool Z80::M_M()  { return R.AF.B.l&S_FLAG; }
inline bool Z80::M_P()  { return !M_M(); }
inline bool Z80::M_PE() { return R.AF.B.l&V_FLAG; }
inline bool Z80::M_PO() { return !M_PE(); }

/* Get next opcode argument and increment program counter */
inline byte Z80::Z80_RDMEM_OPCODE () {
	return Z80_RDOP_ARG(R.PC.W.l++);
}
inline word Z80::Z80_RDMEM_WORD (word A) {
	return Z80_RDMEM(A) + (Z80_RDMEM(((A)+1)&0xFFFF)<<8);
}
inline void Z80::Z80_WRMEM_WORD (word A,word V) {
	Z80_WRMEM (A, V&255);
	Z80_WRMEM (((A)+1)&0xFFFF, V>>8);
}
inline word Z80::Z80_RDMEM_OPCODE_WORD () {
	return Z80_RDMEM_OPCODE() + (Z80_RDMEM_OPCODE()<<8);
}

inline word Z80::M_XIX() { return (R.IX.W.l+(offset)Z80_RDMEM_OPCODE())&0xFFFF; }
inline word Z80::M_XIY() { return (R.IY.W.l+(offset)Z80_RDMEM_OPCODE())&0xFFFF; }
inline byte Z80::M_RD_XHL() { return Z80_RDMEM(R.HL.W.l); }
inline byte Z80::M_RD_XIX() { return Z80_RDMEM(M_XIX()); }
inline byte Z80::M_RD_XIY() { return Z80_RDMEM(M_XIY()); }
inline void Z80::M_WR_XIX(byte a) { Z80_WRMEM(M_XIX(), a); }
inline void Z80::M_WR_XIY(byte a) { Z80_WRMEM(M_XIY(), a); }

#ifdef X86_ASM
#include "Z80CDx86.h"	// this file is missing (and need updates)
#else
#include "Z80Codes.h"
#endif


void Z80::adc_a_xhl() { M_ADC(M_RD_XHL()); }
void Z80::adc_a_xix() { M_ADC(M_RD_XIX()); }
void Z80::adc_a_xiy() { M_ADC(M_RD_XIY()); }
void Z80::adc_a_a()   { M_ADC(R.AF.B.h); }
void Z80::adc_a_b()   { M_ADC(R.BC.B.h); }
void Z80::adc_a_c()   { M_ADC(R.BC.B.l); }
void Z80::adc_a_d()   { M_ADC(R.DE.B.h); }
void Z80::adc_a_e()   { M_ADC(R.DE.B.l); }
void Z80::adc_a_h()   { M_ADC(R.HL.B.h); }
void Z80::adc_a_l()   { M_ADC(R.HL.B.l); }
void Z80::adc_a_ixl() { M_ADC(R.IX.B.l); }
void Z80::adc_a_ixh() { M_ADC(R.IX.B.h); }
void Z80::adc_a_iyl() { M_ADC(R.IY.B.l); }
void Z80::adc_a_iyh() { M_ADC(R.IY.B.h); }
void Z80::adc_a_byte(){ M_ADC(Z80_RDMEM_OPCODE()); }

void Z80::adc_hl_bc() { M_ADCW(R.BC.W.l); }
void Z80::adc_hl_de() { M_ADCW(R.DE.W.l); }
void Z80::adc_hl_hl() { M_ADCW(R.HL.W.l); }
void Z80::adc_hl_sp() { M_ADCW(R.SP.W.l); }

void Z80::add_a_xhl() { M_ADD(M_RD_XHL()); }
void Z80::add_a_xix() { M_ADD(M_RD_XIX()); }
void Z80::add_a_xiy() { M_ADD(M_RD_XIY()); }
void Z80::add_a_a()   { M_ADD(R.AF.B.h); }
void Z80::add_a_b()   { M_ADD(R.BC.B.h); }
void Z80::add_a_c()   { M_ADD(R.BC.B.l); }
void Z80::add_a_d()   { M_ADD(R.DE.B.h); }
void Z80::add_a_e()   { M_ADD(R.DE.B.l); }
void Z80::add_a_h()   { M_ADD(R.HL.B.h); }
void Z80::add_a_l()   { M_ADD(R.HL.B.l); }
void Z80::add_a_ixl() { M_ADD(R.IX.B.l); }
void Z80::add_a_ixh() { M_ADD(R.IX.B.h); }
void Z80::add_a_iyl() { M_ADD(R.IY.B.l); }
void Z80::add_a_iyh() { M_ADD(R.IY.B.h); }
void Z80::add_a_byte(){ M_ADD(Z80_RDMEM_OPCODE()); }

void Z80::add_hl_bc() { M_ADDW(R.HL.W.l, R.BC.W.l); }
void Z80::add_hl_de() { M_ADDW(R.HL.W.l, R.DE.W.l); }
void Z80::add_hl_hl() { M_ADDW(R.HL.W.l, R.HL.W.l); }
void Z80::add_hl_sp() { M_ADDW(R.HL.W.l, R.SP.W.l); }
void Z80::add_ix_bc() { M_ADDW(R.IX.W.l, R.BC.W.l); }
void Z80::add_ix_de() { M_ADDW(R.IX.W.l, R.DE.W.l); }
void Z80::add_ix_ix() { M_ADDW(R.IX.W.l, R.IX.W.l); }
void Z80::add_ix_sp() { M_ADDW(R.IX.W.l, R.SP.W.l); }
void Z80::add_iy_bc() { M_ADDW(R.IY.W.l, R.BC.W.l); }
void Z80::add_iy_de() { M_ADDW(R.IY.W.l, R.DE.W.l); }
void Z80::add_iy_iy() { M_ADDW(R.IY.W.l, R.IY.W.l); }
void Z80::add_iy_sp() { M_ADDW(R.IY.W.l, R.SP.W.l); }

void Z80::and_xhl() { M_AND(M_RD_XHL()); }
void Z80::and_xix() { M_AND(M_RD_XIX()); }
void Z80::and_xiy() { M_AND(M_RD_XIY()); }
void Z80::and_a()   { R.AF.B.l = ZSPTable[R.AF.B.h]|H_FLAG; }
void Z80::and_b()   { M_AND(R.BC.B.h); }
void Z80::and_c()   { M_AND(R.BC.B.l); }
void Z80::and_d()   { M_AND(R.DE.B.h); }
void Z80::and_e()   { M_AND(R.DE.B.l); }
void Z80::and_h()   { M_AND(R.HL.B.h); }
void Z80::and_l()   { M_AND(R.HL.B.l); }
void Z80::and_ixh() { M_AND(R.IX.B.h); }
void Z80::and_ixl() { M_AND(R.IX.B.l); }
void Z80::and_iyh() { M_AND(R.IY.B.h); }
void Z80::and_iyl() { M_AND(R.IY.B.l); }
void Z80::and_byte(){ M_AND(Z80_RDMEM_OPCODE()); }

void Z80::bit_0_xhl() { M_BIT(0, M_RD_XHL()); }
void Z80::bit_0_xix() { M_BIT(0, M_RD_XIX()); }
void Z80::bit_0_xiy() { M_BIT(0, M_RD_XIY()); }
void Z80::bit_0_a()   { M_BIT(0, R.AF.B.h); }
void Z80::bit_0_b()   { M_BIT(0, R.BC.B.h); }
void Z80::bit_0_c()   { M_BIT(0, R.BC.B.l); }
void Z80::bit_0_d()   { M_BIT(0, R.DE.B.h); }
void Z80::bit_0_e()   { M_BIT(0, R.DE.B.l); }
void Z80::bit_0_h()   { M_BIT(0, R.HL.B.h); }
void Z80::bit_0_l()   { M_BIT(0, R.HL.B.l); }

void Z80::bit_1_xhl() { M_BIT(1, M_RD_XHL()); }
void Z80::bit_1_xix() { M_BIT(1, M_RD_XIX()); }
void Z80::bit_1_xiy() { M_BIT(1, M_RD_XIY()); }
void Z80::bit_1_a()   { M_BIT(1, R.AF.B.h); }
void Z80::bit_1_b()   { M_BIT(1, R.BC.B.h); }
void Z80::bit_1_c()   { M_BIT(1, R.BC.B.l); }
void Z80::bit_1_d()   { M_BIT(1, R.DE.B.h); }
void Z80::bit_1_e()   { M_BIT(1, R.DE.B.l); }
void Z80::bit_1_h()   { M_BIT(1, R.HL.B.h); }
void Z80::bit_1_l()   { M_BIT(1, R.HL.B.l); }

void Z80::bit_2_xhl() { M_BIT(2, M_RD_XHL()); }
void Z80::bit_2_xix() { M_BIT(2, M_RD_XIX()); }
void Z80::bit_2_xiy() { M_BIT(2, M_RD_XIY()); }
void Z80::bit_2_a()   { M_BIT(2, R.AF.B.h); }
void Z80::bit_2_b()   { M_BIT(2, R.BC.B.h); }
void Z80::bit_2_c()   { M_BIT(2, R.BC.B.l); }
void Z80::bit_2_d()   { M_BIT(2, R.DE.B.h); }
void Z80::bit_2_e()   { M_BIT(2, R.DE.B.l); }
void Z80::bit_2_h()   { M_BIT(2, R.HL.B.h); }
void Z80::bit_2_l()   { M_BIT(2, R.HL.B.l); }

void Z80::bit_3_xhl() { M_BIT(3, M_RD_XHL()); }
void Z80::bit_3_xix() { M_BIT(3, M_RD_XIX()); }
void Z80::bit_3_xiy() { M_BIT(3, M_RD_XIY()); }
void Z80::bit_3_a()   { M_BIT(3, R.AF.B.h); }
void Z80::bit_3_b()   { M_BIT(3, R.BC.B.h); }
void Z80::bit_3_c()   { M_BIT(3, R.BC.B.l); }
void Z80::bit_3_d()   { M_BIT(3, R.DE.B.h); }
void Z80::bit_3_e()   { M_BIT(3, R.DE.B.l); }
void Z80::bit_3_h()   { M_BIT(3, R.HL.B.h); }
void Z80::bit_3_l()   { M_BIT(3, R.HL.B.l); }

void Z80::bit_4_xhl() { M_BIT(4, M_RD_XHL()); }
void Z80::bit_4_xix() { M_BIT(4, M_RD_XIX()); }
void Z80::bit_4_xiy() { M_BIT(4, M_RD_XIY()); }
void Z80::bit_4_a()   { M_BIT(4, R.AF.B.h); }
void Z80::bit_4_b()   { M_BIT(4, R.BC.B.h); }
void Z80::bit_4_c()   { M_BIT(4, R.BC.B.l); }
void Z80::bit_4_d()   { M_BIT(4, R.DE.B.h); }
void Z80::bit_4_e()   { M_BIT(4, R.DE.B.l); }
void Z80::bit_4_h()   { M_BIT(4, R.HL.B.h); }
void Z80::bit_4_l()   { M_BIT(4, R.HL.B.l); }

void Z80::bit_5_xhl() { M_BIT(5, M_RD_XHL()); }
void Z80::bit_5_xix() { M_BIT(5, M_RD_XIX()); }
void Z80::bit_5_xiy() { M_BIT(5, M_RD_XIY()); }
void Z80::bit_5_a()   { M_BIT(5, R.AF.B.h); }
void Z80::bit_5_b()   { M_BIT(5, R.BC.B.h); }
void Z80::bit_5_c()   { M_BIT(5, R.BC.B.l); }
void Z80::bit_5_d()   { M_BIT(5, R.DE.B.h); }
void Z80::bit_5_e()   { M_BIT(5, R.DE.B.l); }
void Z80::bit_5_h()   { M_BIT(5, R.HL.B.h); }
void Z80::bit_5_l()   { M_BIT(5, R.HL.B.l); }

void Z80::bit_6_xhl() { M_BIT(6, M_RD_XHL()); }
void Z80::bit_6_xix() { M_BIT(6, M_RD_XIX()); }
void Z80::bit_6_xiy() { M_BIT(6, M_RD_XIY()); }
void Z80::bit_6_a()   { M_BIT(6, R.AF.B.h); }
void Z80::bit_6_b()   { M_BIT(6, R.BC.B.h); }
void Z80::bit_6_c()   { M_BIT(6, R.BC.B.l); }
void Z80::bit_6_d()   { M_BIT(6, R.DE.B.h); }
void Z80::bit_6_e()   { M_BIT(6, R.DE.B.l); }
void Z80::bit_6_h()   { M_BIT(6, R.HL.B.h); }
void Z80::bit_6_l()   { M_BIT(6, R.HL.B.l); }

void Z80::bit_7_xhl() { M_BIT(7, M_RD_XHL()); }
void Z80::bit_7_xix() { M_BIT(7, M_RD_XIX()); }
void Z80::bit_7_xiy() { M_BIT(7, M_RD_XIY()); }
void Z80::bit_7_a()   { M_BIT(7, R.AF.B.h); }
void Z80::bit_7_b()   { M_BIT(7, R.BC.B.h); }
void Z80::bit_7_c()   { M_BIT(7, R.BC.B.l); }
void Z80::bit_7_d()   { M_BIT(7, R.DE.B.h); }
void Z80::bit_7_e()   { M_BIT(7, R.DE.B.l); }
void Z80::bit_7_h()   { M_BIT(7, R.HL.B.h); }
void Z80::bit_7_l()   { M_BIT(7, R.HL.B.l); }

void Z80::call_c()  { if (M_C())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_m()  { if (M_M())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_nc() { if (M_NC()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_nz() { if (M_NZ()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_p()  { if (M_P())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_pe() { if (M_PE()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_po() { if (M_PO()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_z()  { if (M_Z())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call()                  { M_CALL(); }

void Z80::ccf() { R.AF.B.l = ((R.AF.B.l&0xED)|((R.AF.B.l&1)<<4))^1; }

void Z80::cp_xhl() { M_CP(M_RD_XHL()); }
void Z80::cp_xix() { M_CP(M_RD_XIX()); }
void Z80::cp_xiy() { M_CP(M_RD_XIY()); }
void Z80::cp_a()   { M_CP(R.AF.B.h); }
void Z80::cp_b()   { M_CP(R.BC.B.h); }
void Z80::cp_c()   { M_CP(R.BC.B.l); }
void Z80::cp_d()   { M_CP(R.DE.B.h); }
void Z80::cp_e()   { M_CP(R.DE.B.l); }
void Z80::cp_h()   { M_CP(R.HL.B.h); }
void Z80::cp_l()   { M_CP(R.HL.B.l); }
void Z80::cp_ixh() { M_CP(R.IX.B.h); }
void Z80::cp_ixl() { M_CP(R.IX.B.l); }
void Z80::cp_iyh() { M_CP(R.IY.B.h); }
void Z80::cp_iyl() { M_CP(R.IY.B.l); }
void Z80::cp_byte(){ M_CP(Z80_RDMEM_OPCODE()); }

void Z80::cpd() {
	byte i = Z80_RDMEM(R.HL.W.l--);
	byte j = R.AF.B.h-i;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[j]|
	           ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}
void Z80::cpdr() {
	cpd ();
	if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

void Z80::cpi() {
	byte i = Z80_RDMEM(R.HL.W.l++);
	byte j = R.AF.B.h-i;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[j]|
	           ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}

void Z80::cpir() {
	cpi ();
	if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

void Z80::cpl() { R.AF.B.h^=0xFF; R.AF.B.l|=(H_FLAG|N_FLAG); }

void Z80::daa() {
	int i = R.AF.B.h;
	if (R.AF.B.l&C_FLAG) i|=256;
	if (R.AF.B.l&H_FLAG) i|=512;
	if (R.AF.B.l&N_FLAG) i|=1024;
	R.AF.W.l = DAATable[i];
}

void Z80::dec_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_DEC(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::dec_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_DEC(i);
	Z80_WRMEM(j, i);
}
void Z80::dec_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_DEC(i);
	Z80_WRMEM(j, i);
}
void Z80::dec_a()   { M_DEC(R.AF.B.h); }
void Z80::dec_b()   { M_DEC(R.BC.B.h); }
void Z80::dec_c()   { M_DEC(R.BC.B.l); }
void Z80::dec_d()   { M_DEC(R.DE.B.h); }
void Z80::dec_e()   { M_DEC(R.DE.B.l); }
void Z80::dec_h()   { M_DEC(R.HL.B.h); }
void Z80::dec_l()   { M_DEC(R.HL.B.l); }
void Z80::dec_ixh() { M_DEC(R.IX.B.h); }
void Z80::dec_ixl() { M_DEC(R.IX.B.l); }
void Z80::dec_iyh() { M_DEC(R.IY.B.h); }
void Z80::dec_iyl() { M_DEC(R.IY.B.l); }

void Z80::dec_bc() { --R.BC.W.l; }
void Z80::dec_de() { --R.DE.W.l; }
void Z80::dec_hl() { --R.HL.W.l; }
void Z80::dec_ix() { --R.IX.W.l; }
void Z80::dec_iy() { --R.IY.W.l; }
void Z80::dec_sp() { --R.SP.W.l; }

void Z80::di() { R.IFF1 = R.nextIFF1 = R.IFF2 = false; }

void Z80::djnz() { if (--R.BC.B.h) { M_JR(); } else { M_SKIP_JR(); } }

void Z80::ex_xsp_hl() {
	int i = Z80_RDMEM_WORD(R.SP.W.l);
	Z80_WRMEM_WORD(R.SP.W.l, R.HL.W.l);
	R.HL.W.l = i;
}
void Z80::ex_xsp_ix() {
	int i = Z80_RDMEM_WORD(R.SP.W.l);
	Z80_WRMEM_WORD(R.SP.W.l, R.IX.W.l);
	R.IX.W.l = i;
}
void Z80::ex_xsp_iy()
{
	int i = Z80_RDMEM_WORD(R.SP.W.l);
	Z80_WRMEM_WORD(R.SP.W.l, R.IY.W.l);
	R.IY.W.l = i;
}
void Z80::ex_af_af() {
	int i = R.AF.W.l;
	R.AF.W.l = R.AF2.W.l;
	R.AF2.W.l = i;
}
void Z80::ex_de_hl() {
	int i = R.DE.W.l;
	R.DE.W.l = R.HL.W.l;
	R.HL.W.l = i;
}
void Z80::exx() {
	int i;
	i = R.BC.W.l; R.BC.W.l = R.BC2.W.l; R.BC2.W.l = i;
	i = R.DE.W.l; R.DE.W.l = R.DE2.W.l; R.DE2.W.l = i;
	i = R.HL.W.l; R.HL.W.l = R.HL2.W.l; R.HL2.W.l = i;
}

void Z80::halt() {
	R.HALT = true;
}

void Z80::im_0() { R.IM = 0; }
void Z80::im_1() { R.IM = 1; }
void Z80::im_2() { R.IM = 2; }

void Z80::in_a_c() { M_IN(R.AF.B.h); }
void Z80::in_b_c() { M_IN(R.BC.B.h); }
void Z80::in_c_c() { M_IN(R.BC.B.l); }
void Z80::in_d_c() { M_IN(R.DE.B.h); }
void Z80::in_e_c() { M_IN(R.DE.B.l); }
void Z80::in_h_c() { M_IN(R.HL.B.h); }
void Z80::in_l_c() { M_IN(R.HL.B.l); }
void Z80::in_0_c() { byte i; M_IN(i); }
void Z80::in_a_byte() { R.AF.B.h = Z80_In(Z80_RDMEM_OPCODE()+(R.AF.B.h<<8)); }

void Z80::inc_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_INC(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::inc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_INC(i);
	Z80_WRMEM(j, i);
}
void Z80::inc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_INC(i);
	Z80_WRMEM(j, i);
}
void Z80::inc_a()   { M_INC(R.AF.B.h); }
void Z80::inc_b()   { M_INC(R.BC.B.h); }
void Z80::inc_c()   { M_INC(R.BC.B.l); }
void Z80::inc_d()   { M_INC(R.DE.B.h); }
void Z80::inc_e()   { M_INC(R.DE.B.l); }
void Z80::inc_h()   { M_INC(R.HL.B.h); }
void Z80::inc_l()   { M_INC(R.HL.B.l); }
void Z80::inc_ixh() { M_INC(R.IX.B.h); }
void Z80::inc_ixl() { M_INC(R.IX.B.l); }
void Z80::inc_iyh() { M_INC(R.IY.B.h); }
void Z80::inc_iyl() { M_INC(R.IY.B.l); }

void Z80::inc_bc() { ++R.BC.W.l; }
void Z80::inc_de() { ++R.DE.W.l; }
void Z80::inc_hl() { ++R.HL.W.l; }
void Z80::inc_ix() { ++R.IX.W.l; }
void Z80::inc_iy() { ++R.IY.W.l; }
void Z80::inc_sp() { ++R.SP.W.l; }

void Z80::ind() {
	--R.BC.B.h;
	Z80_WRMEM(R.HL.W.l, Z80_In(R.BC.B.l+(R.BC.B.h<<8)));
	--R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (N_FLAG|Z_FLAG);
}
void Z80::indr() {
	ind ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

void Z80::ini() {
	--R.BC.B.h;
	Z80_WRMEM(R.HL.W.l, Z80_In(R.BC.B.l+(R.BC.B.h<<8)));
	++R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (N_FLAG|Z_FLAG);
}
void Z80::inir() {
	ini ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

void Z80::jp_hl() { R.PC.W.l = R.HL.W.l; }
void Z80::jp_ix() { R.PC.W.l = R.IX.W.l; }
void Z80::jp_iy() { R.PC.W.l = R.IY.W.l; }
void Z80::jp()                  { M_JP(); }
void Z80::jp_c()  { if (M_C())  { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_m()  { if (M_M())  { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_nc() { if (M_NC()) { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_nz() { if (M_NZ()) { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_p()  { if (M_P())  { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_pe() { if (M_PE()) { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_po() { if (M_PO()) { M_JP(); } else { M_SKIP_JP(); } }
void Z80::jp_z()  { if (M_Z())  { M_JP(); } else { M_SKIP_JP(); } }

void Z80::jr()                { M_JR(); }
void Z80::jr_c()  { if (M_C())  { M_JR(); } else { M_SKIP_JR(); } }
void Z80::jr_nc() { if (M_NC()) { M_JR(); } else { M_SKIP_JR(); } }
void Z80::jr_nz() { if (M_NZ()) { M_JR(); } else { M_SKIP_JR(); } }
void Z80::jr_z()  { if (M_Z())  { M_JR(); } else { M_SKIP_JR(); } }

void Z80::ld_xbc_a()   { Z80_WRMEM(R.BC.W.l, R.AF.B.h); }
void Z80::ld_xde_a()   { Z80_WRMEM(R.DE.W.l, R.AF.B.h); }
void Z80::ld_xhl_a()   { Z80_WRMEM(R.HL.W.l, R.AF.B.h); }
void Z80::ld_xhl_b()   { Z80_WRMEM(R.HL.W.l, R.BC.B.h); }
void Z80::ld_xhl_c()   { Z80_WRMEM(R.HL.W.l, R.BC.B.l); }
void Z80::ld_xhl_d()   { Z80_WRMEM(R.HL.W.l, R.DE.B.h); }
void Z80::ld_xhl_e()   { Z80_WRMEM(R.HL.W.l, R.DE.B.l); }
void Z80::ld_xhl_h()   { Z80_WRMEM(R.HL.W.l, R.HL.B.h); }
void Z80::ld_xhl_l()   { Z80_WRMEM(R.HL.W.l, R.HL.B.l); }
void Z80::ld_xhl_byte(){ Z80_WRMEM(R.HL.W.l, Z80_RDMEM_OPCODE()); }
void Z80::ld_xix_a()   { M_WR_XIX(R.AF.B.h); }
void Z80::ld_xix_b()   { M_WR_XIX(R.BC.B.h); }
void Z80::ld_xix_c()   { M_WR_XIX(R.BC.B.l); }
void Z80::ld_xix_d()   { M_WR_XIX(R.DE.B.h); }
void Z80::ld_xix_e()   { M_WR_XIX(R.DE.B.l); }
void Z80::ld_xix_h()   { M_WR_XIX(R.HL.B.h); }
void Z80::ld_xix_l()   { M_WR_XIX(R.HL.B.l); }
void Z80::ld_xix_byte(){ Z80_WRMEM(M_XIX(), Z80_RDMEM_OPCODE()); }
void Z80::ld_xiy_a()   { M_WR_XIY(R.AF.B.h); }
void Z80::ld_xiy_b()   { M_WR_XIY(R.BC.B.h); }
void Z80::ld_xiy_c()   { M_WR_XIY(R.BC.B.l); }
void Z80::ld_xiy_d()   { M_WR_XIY(R.DE.B.h); }
void Z80::ld_xiy_e()   { M_WR_XIY(R.DE.B.l); }
void Z80::ld_xiy_h()   { M_WR_XIY(R.HL.B.h); }
void Z80::ld_xiy_l()   { M_WR_XIY(R.HL.B.l); }
void Z80::ld_xiy_byte(){ Z80_WRMEM(M_XIY(), Z80_RDMEM_OPCODE()); }
void Z80::ld_xbyte_a() { Z80_WRMEM(Z80_RDMEM_OPCODE_WORD(), R.AF.B.h); }
void Z80::ld_xword_bc(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.BC.W.l); }
void Z80::ld_xword_de(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.DE.W.l); }
void Z80::ld_xword_hl(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.HL.W.l); }
void Z80::ld_xword_ix(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.IX.W.l); }
void Z80::ld_xword_iy(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.IY.W.l); }
void Z80::ld_xword_sp(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.SP.W.l); }
void Z80::ld_a_xbc()   { R.AF.B.h = Z80_RDMEM(R.BC.W.l); }
void Z80::ld_a_xde()   { R.AF.B.h = Z80_RDMEM(R.DE.W.l); }
void Z80::ld_a_xhl()   { R.AF.B.h = M_RD_XHL(); }
void Z80::ld_a_xix()   { R.AF.B.h = M_RD_XIX(); }
void Z80::ld_a_xiy()   { R.AF.B.h = M_RD_XIY(); }
void Z80::ld_a_xbyte() { R.AF.B.h = Z80_RDMEM(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_a_byte()  { R.AF.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_b_byte()  { R.BC.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_c_byte()  { R.BC.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_d_byte()  { R.DE.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_e_byte()  { R.DE.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_h_byte()  { R.HL.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_l_byte()  { R.HL.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_ixh_byte(){ R.IX.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_ixl_byte(){ R.IX.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_iyh_byte(){ R.IY.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_iyl_byte(){ R.IY.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_b_xhl()   { R.BC.B.h = M_RD_XHL(); }
void Z80::ld_c_xhl()   { R.BC.B.l = M_RD_XHL(); }
void Z80::ld_d_xhl()   { R.DE.B.h = M_RD_XHL(); }
void Z80::ld_e_xhl()   { R.DE.B.l = M_RD_XHL(); }
void Z80::ld_h_xhl()   { R.HL.B.h = M_RD_XHL(); }
void Z80::ld_l_xhl()   { R.HL.B.l = M_RD_XHL(); }
void Z80::ld_b_xix()   { R.BC.B.h = M_RD_XIX(); }
void Z80::ld_c_xix()   { R.BC.B.l = M_RD_XIX(); }
void Z80::ld_d_xix()   { R.DE.B.h = M_RD_XIX(); }
void Z80::ld_e_xix()   { R.DE.B.l = M_RD_XIX(); }
void Z80::ld_h_xix()   { R.HL.B.h = M_RD_XIX(); }
void Z80::ld_l_xix()   { R.HL.B.l = M_RD_XIX(); }
void Z80::ld_b_xiy()   { R.BC.B.h = M_RD_XIY(); }
void Z80::ld_c_xiy()   { R.BC.B.l = M_RD_XIY(); }
void Z80::ld_d_xiy()   { R.DE.B.h = M_RD_XIY(); }
void Z80::ld_e_xiy()   { R.DE.B.l = M_RD_XIY(); }
void Z80::ld_h_xiy()   { R.HL.B.h = M_RD_XIY(); }
void Z80::ld_l_xiy()   { R.HL.B.l = M_RD_XIY(); }
void Z80::ld_a_a()     { }
void Z80::ld_a_b()     { R.AF.B.h = R.BC.B.h; }
void Z80::ld_a_c()     { R.AF.B.h = R.BC.B.l; }
void Z80::ld_a_d()     { R.AF.B.h = R.DE.B.h; }
void Z80::ld_a_e()     { R.AF.B.h = R.DE.B.l; }
void Z80::ld_a_h()     { R.AF.B.h = R.HL.B.h; }
void Z80::ld_a_l()     { R.AF.B.h = R.HL.B.l; }
void Z80::ld_a_ixh()   { R.AF.B.h = R.IX.B.h; }
void Z80::ld_a_ixl()   { R.AF.B.h = R.IX.B.l; }
void Z80::ld_a_iyh()   { R.AF.B.h = R.IY.B.h; }
void Z80::ld_a_iyl()   { R.AF.B.h = R.IY.B.l; }
void Z80::ld_b_b()     { }
void Z80::ld_b_a()     { R.BC.B.h = R.AF.B.h; }
void Z80::ld_b_c()     { R.BC.B.h = R.BC.B.l; }
void Z80::ld_b_d()     { R.BC.B.h = R.DE.B.h; }
void Z80::ld_b_e()     { R.BC.B.h = R.DE.B.l; }
void Z80::ld_b_h()     { R.BC.B.h = R.HL.B.h; }
void Z80::ld_b_l()     { R.BC.B.h = R.HL.B.l; }
void Z80::ld_b_ixh()   { R.BC.B.h = R.IX.B.h; }
void Z80::ld_b_ixl()   { R.BC.B.h = R.IX.B.l; }
void Z80::ld_b_iyh()   { R.BC.B.h = R.IY.B.h; }
void Z80::ld_b_iyl()   { R.BC.B.h = R.IY.B.l; }
void Z80::ld_c_c()     { }
void Z80::ld_c_a()     { R.BC.B.l = R.AF.B.h; }
void Z80::ld_c_b()     { R.BC.B.l = R.BC.B.h; }
void Z80::ld_c_d()     { R.BC.B.l = R.DE.B.h; }
void Z80::ld_c_e()     { R.BC.B.l = R.DE.B.l; }
void Z80::ld_c_h()     { R.BC.B.l = R.HL.B.h; }
void Z80::ld_c_l()     { R.BC.B.l = R.HL.B.l; }
void Z80::ld_c_ixh()   { R.BC.B.l = R.IX.B.h; }
void Z80::ld_c_ixl()   { R.BC.B.l = R.IX.B.l; }
void Z80::ld_c_iyh()   { R.BC.B.l = R.IY.B.h; }
void Z80::ld_c_iyl()   { R.BC.B.l = R.IY.B.l; }
void Z80::ld_d_d()     { }
void Z80::ld_d_a()     { R.DE.B.h = R.AF.B.h; }
void Z80::ld_d_c()     { R.DE.B.h = R.BC.B.l; }
void Z80::ld_d_b()     { R.DE.B.h = R.BC.B.h; }
void Z80::ld_d_e()     { R.DE.B.h = R.DE.B.l; }
void Z80::ld_d_h()     { R.DE.B.h = R.HL.B.h; }
void Z80::ld_d_l()     { R.DE.B.h = R.HL.B.l; }
void Z80::ld_d_ixh()   { R.DE.B.h = R.IX.B.h; }
void Z80::ld_d_ixl()   { R.DE.B.h = R.IX.B.l; }
void Z80::ld_d_iyh()   { R.DE.B.h = R.IY.B.h; }
void Z80::ld_d_iyl()   { R.DE.B.h = R.IY.B.l; }
void Z80::ld_e_e()     { }
void Z80::ld_e_a()     { R.DE.B.l = R.AF.B.h; }
void Z80::ld_e_c()     { R.DE.B.l = R.BC.B.l; }
void Z80::ld_e_b()     { R.DE.B.l = R.BC.B.h; }
void Z80::ld_e_d()     { R.DE.B.l = R.DE.B.h; }
void Z80::ld_e_h()     { R.DE.B.l = R.HL.B.h; }
void Z80::ld_e_l()     { R.DE.B.l = R.HL.B.l; }
void Z80::ld_e_ixh()   { R.DE.B.l = R.IX.B.h; }
void Z80::ld_e_ixl()   { R.DE.B.l = R.IX.B.l; }
void Z80::ld_e_iyh()   { R.DE.B.l = R.IY.B.h; }
void Z80::ld_e_iyl()   { R.DE.B.l = R.IY.B.l; }
void Z80::ld_h_h()     { }
void Z80::ld_h_a()     { R.HL.B.h = R.AF.B.h; }
void Z80::ld_h_c()     { R.HL.B.h = R.BC.B.l; }
void Z80::ld_h_b()     { R.HL.B.h = R.BC.B.h; }
void Z80::ld_h_e()     { R.HL.B.h = R.DE.B.l; }
void Z80::ld_h_d()     { R.HL.B.h = R.DE.B.h; }
void Z80::ld_h_l()     { R.HL.B.h = R.HL.B.l; }
void Z80::ld_l_l()     { }
void Z80::ld_l_a()     { R.HL.B.l = R.AF.B.h; }
void Z80::ld_l_c()     { R.HL.B.l = R.BC.B.l; }
void Z80::ld_l_b()     { R.HL.B.l = R.BC.B.h; }
void Z80::ld_l_e()     { R.HL.B.l = R.DE.B.l; }
void Z80::ld_l_d()     { R.HL.B.l = R.DE.B.h; }
void Z80::ld_l_h()     { R.HL.B.l = R.HL.B.h; }
void Z80::ld_ixh_a()   { R.IX.B.h = R.AF.B.h; }
void Z80::ld_ixh_b()   { R.IX.B.h = R.BC.B.h; }
void Z80::ld_ixh_c()   { R.IX.B.h = R.BC.B.l; }
void Z80::ld_ixh_d()   { R.IX.B.h = R.DE.B.h; }
void Z80::ld_ixh_e()   { R.IX.B.h = R.DE.B.l; }
void Z80::ld_ixh_ixh() { }
void Z80::ld_ixh_ixl() { R.IX.B.h = R.IX.B.l; }
void Z80::ld_ixl_a()   { R.IX.B.l = R.AF.B.h; }
void Z80::ld_ixl_b()   { R.IX.B.l = R.BC.B.h; }
void Z80::ld_ixl_c()   { R.IX.B.l = R.BC.B.l; }
void Z80::ld_ixl_d()   { R.IX.B.l = R.DE.B.h; }
void Z80::ld_ixl_e()   { R.IX.B.l = R.DE.B.l; }
void Z80::ld_ixl_ixh() { R.IX.B.l = R.IX.B.h; }
void Z80::ld_ixl_ixl() { }
void Z80::ld_iyh_a()   { R.IY.B.h = R.AF.B.h; }
void Z80::ld_iyh_b()   { R.IY.B.h = R.BC.B.h; }
void Z80::ld_iyh_c()   { R.IY.B.h = R.BC.B.l; }
void Z80::ld_iyh_d()   { R.IY.B.h = R.DE.B.h; }
void Z80::ld_iyh_e()   { R.IY.B.h = R.DE.B.l; }
void Z80::ld_iyh_iyh() { }
void Z80::ld_iyh_iyl() { R.IY.B.h = R.IY.B.l; }
void Z80::ld_iyl_a()   { R.IY.B.l = R.AF.B.h; }
void Z80::ld_iyl_b()   { R.IY.B.l = R.BC.B.h; }
void Z80::ld_iyl_c()   { R.IY.B.l = R.BC.B.l; }
void Z80::ld_iyl_d()   { R.IY.B.l = R.DE.B.h; }
void Z80::ld_iyl_e()   { R.IY.B.l = R.DE.B.l; }
void Z80::ld_iyl_iyh() { R.IY.B.l = R.IY.B.h; }
void Z80::ld_iyl_iyl() { }
void Z80::ld_bc_xword(){ R.BC.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_bc_word() { R.BC.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_de_xword(){ R.DE.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_de_word() { R.DE.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_hl_xword(){ R.HL.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_hl_word() { R.HL.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_ix_xword(){ R.IX.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_ix_word() { R.IX.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_iy_xword(){ R.IY.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_iy_word() { R.IY.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_xword(){ R.SP.W.l = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_sp_word() { R.SP.W.l = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_hl()   { R.SP.W.l = R.HL.W.l; }
void Z80::ld_sp_ix()   { R.SP.W.l = R.IX.W.l; }
void Z80::ld_sp_iy()   { R.SP.W.l = R.IY.W.l; }
void Z80::ld_a_i() {
	R.AF.B.h = R.I;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[R.I]|((R.IFF2 ? 1 : 0)<<2);
}
void Z80::ld_i_a()     { R.I = R.AF.B.h; }
void Z80::ld_a_r() {
	R.AF.B.h=(R.R&127)|(R.R2&128);
	R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.AF.B.h]|((R.IFF2 ? 1 : 0)<<2);
}
void Z80::ld_r_a()     { R.R = R.R2 = R.AF.B.h; }

void Z80::ldd() {
	Z80_WRMEM(R.DE.W.l,Z80_RDMEM(R.HL.W.l));
	--R.DE.W.l;
	--R.HL.W.l;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&0xE9)|(R.BC.W.l ? V_FLAG : 0);
}
void Z80::lddr() {
	ldd ();
	if (R.BC.W.l) { R.ICount += 5; R.PC.W.l -= 2; }
}
void Z80::ldi() {
	Z80_WRMEM(R.DE.W.l,Z80_RDMEM(R.HL.W.l));
	++R.DE.W.l;
	++R.HL.W.l;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&0xE9)|(R.BC.W.l ? V_FLAG : 0);
}
void Z80::ldir() {
	ldi ();
	if (R.BC.W.l) { R.ICount += 5; R.PC.W.l -= 2; }
}

void Z80::neg() {
	 byte i = R.AF.B.h;
	 R.AF.B.h = 0;
	 M_SUB(i);
}

void Z80::nop() { };

void Z80::or_xhl() { M_OR(M_RD_XHL()); }
void Z80::or_xix() { M_OR(M_RD_XIX()); }
void Z80::or_xiy() { M_OR(M_RD_XIY()); }
void Z80::or_a()   { R.AF.B.l = ZSPTable[R.AF.B.h]; }
void Z80::or_b()   { M_OR(R.BC.B.h); }
void Z80::or_c()   { M_OR(R.BC.B.l); }
void Z80::or_d()   { M_OR(R.DE.B.h); }
void Z80::or_e()   { M_OR(R.DE.B.l); }
void Z80::or_h()   { M_OR(R.HL.B.h); }
void Z80::or_l()   { M_OR(R.HL.B.l); }
void Z80::or_ixh() { M_OR(R.IX.B.h); }
void Z80::or_ixl() { M_OR(R.IX.B.l); }
void Z80::or_iyh() { M_OR(R.IY.B.h); }
void Z80::or_iyl() { M_OR(R.IY.B.l); }
void Z80::or_byte(){ M_OR(Z80_RDMEM_OPCODE()); }

void Z80::outd() {
	--R.BC.B.h;
	Z80_Out (R.BC.B.l+(R.BC.B.h<<8), Z80_RDMEM(R.HL.W.l));
	--R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (Z_FLAG|N_FLAG);
}
void Z80::otdr() {
	outd ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}
void Z80::outi() {
	--R.BC.B.h;
	Z80_Out (R.BC.B.l+(R.BC.B.h<<8), Z80_RDMEM(R.HL.W.l));
	++R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (Z_FLAG|N_FLAG);
}
void Z80::otir() {
	outi ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

void Z80::out_c_a()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.AF.B.h); }
void Z80::out_c_b()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.BC.B.h); }
void Z80::out_c_c()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.BC.B.l); }
void Z80::out_c_d()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.DE.B.h); }
void Z80::out_c_e()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.DE.B.l); }
void Z80::out_c_h()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.HL.B.h); }
void Z80::out_c_l()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), R.HL.B.l); }
void Z80::out_c_0()   { Z80_Out(R.BC.B.l+(R.BC.B.h<<8), 0); }
void Z80::out_byte_a(){ Z80_Out(Z80_RDMEM_OPCODE()+(R.AF.B.h<<8), R.AF.B.h); }

void Z80::pop_af() { M_POP(R.AF.W.l); }
void Z80::pop_bc() { M_POP(R.BC.W.l); }
void Z80::pop_de() { M_POP(R.DE.W.l); }
void Z80::pop_hl() { M_POP(R.HL.W.l); }
void Z80::pop_ix() { M_POP(R.IX.W.l); }
void Z80::pop_iy() { M_POP(R.IY.W.l); }

void Z80::push_af() { M_PUSH(R.AF.W.l); }
void Z80::push_bc() { M_PUSH(R.BC.W.l); }
void Z80::push_de() { M_PUSH(R.DE.W.l); }
void Z80::push_hl() { M_PUSH(R.HL.W.l); }
void Z80::push_ix() { M_PUSH(R.IX.W.l); }
void Z80::push_iy() { M_PUSH(R.IY.W.l); }

void Z80::res_0_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(0, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	Z80_WRMEM(j, i);
}
void Z80::res_0_a() { M_RES(0, R.AF.B.h); };
void Z80::res_0_b() { M_RES(0, R.BC.B.h); };
void Z80::res_0_c() { M_RES(0, R.BC.B.l); };
void Z80::res_0_d() { M_RES(0, R.DE.B.h); };
void Z80::res_0_e() { M_RES(0, R.DE.B.l); };
void Z80::res_0_h() { M_RES(0, R.HL.B.h); };
void Z80::res_0_l() { M_RES(0, R.HL.B.l); };

void Z80::res_1_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(1, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	Z80_WRMEM(j, i);
}
void Z80::res_1_a() { M_RES(1, R.AF.B.h); };
void Z80::res_1_b() { M_RES(1, R.BC.B.h); };
void Z80::res_1_c() { M_RES(1, R.BC.B.l); };
void Z80::res_1_d() { M_RES(1, R.DE.B.h); };
void Z80::res_1_e() { M_RES(1, R.DE.B.l); };
void Z80::res_1_h() { M_RES(1, R.HL.B.h); };
void Z80::res_1_l() { M_RES(1, R.HL.B.l); };

void Z80::res_2_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(2, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	Z80_WRMEM(j, i);
}
void Z80::res_2_a() { M_RES(2, R.AF.B.h); };
void Z80::res_2_b() { M_RES(2, R.BC.B.h); };
void Z80::res_2_c() { M_RES(2, R.BC.B.l); };
void Z80::res_2_d() { M_RES(2, R.DE.B.h); };
void Z80::res_2_e() { M_RES(2, R.DE.B.l); };
void Z80::res_2_h() { M_RES(2, R.HL.B.h); };
void Z80::res_2_l() { M_RES(2, R.HL.B.l); };

void Z80::res_3_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(3, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	Z80_WRMEM(j, i);
}
void Z80::res_3_a() { M_RES(3, R.AF.B.h); };
void Z80::res_3_b() { M_RES(3, R.BC.B.h); };
void Z80::res_3_c() { M_RES(3, R.BC.B.l); };
void Z80::res_3_d() { M_RES(3, R.DE.B.h); };
void Z80::res_3_e() { M_RES(3, R.DE.B.l); };
void Z80::res_3_h() { M_RES(3, R.HL.B.h); };
void Z80::res_3_l() { M_RES(3, R.HL.B.l); };

void Z80::res_4_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(4, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	Z80_WRMEM(j, i);
}
void Z80::res_4_a() { M_RES(4, R.AF.B.h); };
void Z80::res_4_b() { M_RES(4, R.BC.B.h); };
void Z80::res_4_c() { M_RES(4, R.BC.B.l); };
void Z80::res_4_d() { M_RES(4, R.DE.B.h); };
void Z80::res_4_e() { M_RES(4, R.DE.B.l); };
void Z80::res_4_h() { M_RES(4, R.HL.B.h); };
void Z80::res_4_l() { M_RES(4, R.HL.B.l); };

void Z80::res_5_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(5, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	Z80_WRMEM(j, i);
}
void Z80::res_5_a() { M_RES(5, R.AF.B.h); };
void Z80::res_5_b() { M_RES(5, R.BC.B.h); };
void Z80::res_5_c() { M_RES(5, R.BC.B.l); };
void Z80::res_5_d() { M_RES(5, R.DE.B.h); };
void Z80::res_5_e() { M_RES(5, R.DE.B.l); };
void Z80::res_5_h() { M_RES(5, R.HL.B.h); };
void Z80::res_5_l() { M_RES(5, R.HL.B.l); };

void Z80::res_6_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(6, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	Z80_WRMEM(j, i);
}
void Z80::res_6_a() { M_RES(6, R.AF.B.h); };
void Z80::res_6_b() { M_RES(6, R.BC.B.h); };
void Z80::res_6_c() { M_RES(6, R.BC.B.l); };
void Z80::res_6_d() { M_RES(6, R.DE.B.h); };
void Z80::res_6_e() { M_RES(6, R.DE.B.l); };
void Z80::res_6_h() { M_RES(6, R.HL.B.h); };
void Z80::res_6_l() { M_RES(6, R.HL.B.l); };

void Z80::res_7_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RES(7, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::res_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	Z80_WRMEM(j, i);
}
void Z80::res_7_a() { M_RES(7, R.AF.B.h); };
void Z80::res_7_b() { M_RES(7, R.BC.B.h); };
void Z80::res_7_c() { M_RES(7, R.BC.B.l); };
void Z80::res_7_d() { M_RES(7, R.DE.B.h); };
void Z80::res_7_e() { M_RES(7, R.DE.B.l); };
void Z80::res_7_h() { M_RES(7, R.HL.B.h); };
void Z80::res_7_l() { M_RES(7, R.HL.B.l); };

void Z80::ret()                  { M_RET(); }
void Z80::ret_c()  { if (M_C())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_m()  { if (M_M())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_nc() { if (M_NC()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_nz() { if (M_NZ()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_p()  { if (M_P())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_pe() { if (M_PE()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_po() { if (M_PO()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_z()  { if (M_Z())  { M_RET(); } else { M_SKIP_RET(); } }

void Z80::reti() { interface->Z80_Reti(); M_RET(); }
void Z80::retn() { R.IFF1 = R.nextIFF1 = R.IFF2; interface->Z80_Retn(); M_RET(); }

void Z80::rl_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RL(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::rl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	Z80_WRMEM(j, i);
}
void Z80::rl_a() { M_RL(R.AF.B.h); }
void Z80::rl_b() { M_RL(R.BC.B.h); }
void Z80::rl_c() { M_RL(R.BC.B.l); }
void Z80::rl_d() { M_RL(R.DE.B.h); }
void Z80::rl_e() { M_RL(R.DE.B.l); }
void Z80::rl_h() { M_RL(R.HL.B.h); }
void Z80::rl_l() { M_RL(R.HL.B.l); }
void Z80::rla()  { M_RLA(); }

void Z80::rlc_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RLC(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::rlc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	Z80_WRMEM(j, i);
}
void Z80::rlc_a() { M_RLC(R.AF.B.h); }
void Z80::rlc_b() { M_RLC(R.BC.B.h); }
void Z80::rlc_c() { M_RLC(R.BC.B.l); }
void Z80::rlc_d() { M_RLC(R.DE.B.h); }
void Z80::rlc_e() { M_RLC(R.DE.B.l); }
void Z80::rlc_h() { M_RLC(R.HL.B.h); }
void Z80::rlc_l() { M_RLC(R.HL.B.l); }
void Z80::rlca()  { M_RLCA(); }

void Z80::rld() {
	byte i = Z80_RDMEM(R.HL.W.l);
	Z80_WRMEM(R.HL.W.l, (i<<4)|(R.AF.B.h&0x0F));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i>>4);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

void Z80::rr_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RR(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::rr_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	Z80_WRMEM(j, i);
}
void Z80::rr_a() { M_RR(R.AF.B.h); }
void Z80::rr_b() { M_RR(R.BC.B.h); }
void Z80::rr_c() { M_RR(R.BC.B.l); }
void Z80::rr_d() { M_RR(R.DE.B.h); }
void Z80::rr_e() { M_RR(R.DE.B.l); }
void Z80::rr_h() { M_RR(R.HL.B.h); }
void Z80::rr_l() { M_RR(R.HL.B.l); }
void Z80::rra()  { M_RRA(); }

void Z80::rrc_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_RRC(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::rrc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	Z80_WRMEM(j, i);
}
void Z80::rrc_a() { M_RRC(R.AF.B.h); }
void Z80::rrc_b() { M_RRC(R.BC.B.h); }
void Z80::rrc_c() { M_RRC(R.BC.B.l); }
void Z80::rrc_d() { M_RRC(R.DE.B.h); }
void Z80::rrc_e() { M_RRC(R.DE.B.l); }
void Z80::rrc_h() { M_RRC(R.HL.B.h); }
void Z80::rrc_l() { M_RRC(R.HL.B.l); }
void Z80::rrca()  { M_RRCA(); }

void Z80::rrd() {
	byte i = Z80_RDMEM(R.HL.W.l);
	Z80_WRMEM(R.HL.W.l, (i>>4)|(R.AF.B.h<<4));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i&0x0F);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

void Z80::rst_00() { M_RST(0x00); }
void Z80::rst_08() { M_RST(0x08); }
void Z80::rst_10() { M_RST(0x10); }
void Z80::rst_18() { M_RST(0x18); }
void Z80::rst_20() { M_RST(0x20); }
void Z80::rst_28() { M_RST(0x28); }
void Z80::rst_30() { M_RST(0x30); }
void Z80::rst_38() { M_RST(0x38); }

void Z80::sbc_a_byte(){ M_SBC(Z80_RDMEM_OPCODE()); }
void Z80::sbc_a_xhl() { M_SBC(M_RD_XHL()); }
void Z80::sbc_a_xix() { M_SBC(M_RD_XIX()); }
void Z80::sbc_a_xiy() { M_SBC(M_RD_XIY()); }
void Z80::sbc_a_a()   { M_SBC(R.AF.B.h); }
void Z80::sbc_a_b()   { M_SBC(R.BC.B.h); }
void Z80::sbc_a_c()   { M_SBC(R.BC.B.l); }
void Z80::sbc_a_d()   { M_SBC(R.DE.B.h); }
void Z80::sbc_a_e()   { M_SBC(R.DE.B.l); }
void Z80::sbc_a_h()   { M_SBC(R.HL.B.h); }
void Z80::sbc_a_l()   { M_SBC(R.HL.B.l); }
void Z80::sbc_a_ixh() { M_SBC(R.IX.B.h); }
void Z80::sbc_a_ixl() { M_SBC(R.IX.B.l); }
void Z80::sbc_a_iyh() { M_SBC(R.IY.B.h); }
void Z80::sbc_a_iyl() { M_SBC(R.IY.B.l); }

void Z80::sbc_hl_bc() { M_SBCW(R.BC.W.l); }
void Z80::sbc_hl_de() { M_SBCW(R.DE.W.l); }
void Z80::sbc_hl_hl() { M_SBCW(R.HL.W.l); }
void Z80::sbc_hl_sp() { M_SBCW(R.SP.W.l); }

void Z80::scf() { R.AF.B.l = (R.AF.B.l&0xEC)|C_FLAG; }

void Z80::set_0_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(0, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	Z80_WRMEM(j, i);
}
void Z80::set_0_a() { M_SET(0, R.AF.B.h); }
void Z80::set_0_b() { M_SET(0, R.BC.B.h); }
void Z80::set_0_c() { M_SET(0, R.BC.B.l); }
void Z80::set_0_d() { M_SET(0, R.DE.B.h); }
void Z80::set_0_e() { M_SET(0, R.DE.B.l); }
void Z80::set_0_h() { M_SET(0, R.HL.B.h); }
void Z80::set_0_l() { M_SET(0, R.HL.B.l); }

void Z80::set_1_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(1, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	Z80_WRMEM(j, i);
}
void Z80::set_1_a() { M_SET(1, R.AF.B.h); }
void Z80::set_1_b() { M_SET(1, R.BC.B.h); }
void Z80::set_1_c() { M_SET(1, R.BC.B.l); }
void Z80::set_1_d() { M_SET(1, R.DE.B.h); }
void Z80::set_1_e() { M_SET(1, R.DE.B.l); }
void Z80::set_1_h() { M_SET(1, R.HL.B.h); }
void Z80::set_1_l() { M_SET(1, R.HL.B.l); }

void Z80::set_2_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(2, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	Z80_WRMEM(j, i);
}
void Z80::set_2_a() { M_SET(2, R.AF.B.h); }
void Z80::set_2_b() { M_SET(2, R.BC.B.h); }
void Z80::set_2_c() { M_SET(2, R.BC.B.l); }
void Z80::set_2_d() { M_SET(2, R.DE.B.h); }
void Z80::set_2_e() { M_SET(2, R.DE.B.l); }
void Z80::set_2_h() { M_SET(2, R.HL.B.h); }
void Z80::set_2_l() { M_SET(2, R.HL.B.l); }

void Z80::set_3_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(3, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	Z80_WRMEM(j, i);
}
void Z80::set_3_a() { M_SET(3, R.AF.B.h); }
void Z80::set_3_b() { M_SET(3, R.BC.B.h); }
void Z80::set_3_c() { M_SET(3, R.BC.B.l); }
void Z80::set_3_d() { M_SET(3, R.DE.B.h); }
void Z80::set_3_e() { M_SET(3, R.DE.B.l); }
void Z80::set_3_h() { M_SET(3, R.HL.B.h); }
void Z80::set_3_l() { M_SET(3, R.HL.B.l); }

void Z80::set_4_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(4, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	Z80_WRMEM(j, i);
}
void Z80::set_4_a() { M_SET(4, R.AF.B.h); }
void Z80::set_4_b() { M_SET(4, R.BC.B.h); }
void Z80::set_4_c() { M_SET(4, R.BC.B.l); }
void Z80::set_4_d() { M_SET(4, R.DE.B.h); }
void Z80::set_4_e() { M_SET(4, R.DE.B.l); }
void Z80::set_4_h() { M_SET(4, R.HL.B.h); }
void Z80::set_4_l() { M_SET(4, R.HL.B.l); }

void Z80::set_5_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(5, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	Z80_WRMEM(j, i);
}
void Z80::set_5_a() { M_SET(5, R.AF.B.h); }
void Z80::set_5_b() { M_SET(5, R.BC.B.h); }
void Z80::set_5_c() { M_SET(5, R.BC.B.l); }
void Z80::set_5_d() { M_SET(5, R.DE.B.h); }
void Z80::set_5_e() { M_SET(5, R.DE.B.l); }
void Z80::set_5_h() { M_SET(5, R.HL.B.h); }
void Z80::set_5_l() { M_SET(5, R.HL.B.l); }

void Z80::set_6_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(6, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	Z80_WRMEM(j, i);
}
void Z80::set_6_a() { M_SET(6, R.AF.B.h); }
void Z80::set_6_b() { M_SET(6, R.BC.B.h); }
void Z80::set_6_c() { M_SET(6, R.BC.B.l); }
void Z80::set_6_d() { M_SET(6, R.DE.B.h); }
void Z80::set_6_e() { M_SET(6, R.DE.B.l); }
void Z80::set_6_h() { M_SET(6, R.HL.B.h); }
void Z80::set_6_l() { M_SET(6, R.HL.B.l); }

void Z80::set_7_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SET(7, i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::set_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	Z80_WRMEM(j, i);
}
void Z80::set_7_a() { M_SET(7, R.AF.B.h); }
void Z80::set_7_b() { M_SET(7, R.BC.B.h); }
void Z80::set_7_c() { M_SET(7, R.BC.B.l); }
void Z80::set_7_d() { M_SET(7, R.DE.B.h); }
void Z80::set_7_e() { M_SET(7, R.DE.B.l); }
void Z80::set_7_h() { M_SET(7, R.HL.B.h); }
void Z80::set_7_l() { M_SET(7, R.HL.B.l); }

void Z80::sla_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SLA(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::sla_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	Z80_WRMEM(j, i);
}
void Z80::sla_a() { M_SLA(R.AF.B.h); }
void Z80::sla_b() { M_SLA(R.BC.B.h); }
void Z80::sla_c() { M_SLA(R.BC.B.l); }
void Z80::sla_d() { M_SLA(R.DE.B.h); }
void Z80::sla_e() { M_SLA(R.DE.B.l); }
void Z80::sla_h() { M_SLA(R.HL.B.h); }
void Z80::sla_l() { M_SLA(R.HL.B.l); }

void Z80::sll_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SLL(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::sll_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	Z80_WRMEM(j, i);
}
void Z80::sll_a() { M_SLL(R.AF.B.h); }
void Z80::sll_b() { M_SLL(R.BC.B.h); }
void Z80::sll_c() { M_SLL(R.BC.B.l); }
void Z80::sll_d() { M_SLL(R.DE.B.h); }
void Z80::sll_e() { M_SLL(R.DE.B.l); }
void Z80::sll_h() { M_SLL(R.HL.B.h); }
void Z80::sll_l() { M_SLL(R.HL.B.l); }

void Z80::sra_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SRA(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::sra_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	Z80_WRMEM(j, i);
}
void Z80::sra_a() { M_SRA(R.AF.B.h); }
void Z80::sra_b() { M_SRA(R.BC.B.h); }
void Z80::sra_c() { M_SRA(R.BC.B.l); }
void Z80::sra_d() { M_SRA(R.DE.B.h); }
void Z80::sra_e() { M_SRA(R.DE.B.l); }
void Z80::sra_h() { M_SRA(R.HL.B.h); }
void Z80::sra_l() { M_SRA(R.HL.B.l); }

void Z80::srl_xhl() {
	byte i = Z80_RDMEM(R.HL.W.l);
	M_SRL(i);
	Z80_WRMEM(R.HL.W.l, i);
}
void Z80::srl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	Z80_WRMEM(j, i);
}
void Z80::srl_a() { M_SRL(R.AF.B.h); }
void Z80::srl_b() { M_SRL(R.BC.B.h); }
void Z80::srl_c() { M_SRL(R.BC.B.l); }
void Z80::srl_d() { M_SRL(R.DE.B.h); }
void Z80::srl_e() { M_SRL(R.DE.B.l); }
void Z80::srl_h() { M_SRL(R.HL.B.h); }
void Z80::srl_l() { M_SRL(R.HL.B.l); }

void Z80::sub_xhl() { M_SUB(M_RD_XHL()); }
void Z80::sub_xix() { M_SUB(M_RD_XIX()); }
void Z80::sub_xiy() { M_SUB(M_RD_XIY()); }
void Z80::sub_a()   { R.AF.W.l = Z_FLAG|N_FLAG; }
void Z80::sub_b()   { M_SUB(R.BC.B.h); }
void Z80::sub_c()   { M_SUB(R.BC.B.l); }
void Z80::sub_d()   { M_SUB(R.DE.B.h); }
void Z80::sub_e()   { M_SUB(R.DE.B.l); }
void Z80::sub_h()   { M_SUB(R.HL.B.h); }
void Z80::sub_l()   { M_SUB(R.HL.B.l); }
void Z80::sub_ixh() { M_SUB(R.IX.B.h); }
void Z80::sub_ixl() { M_SUB(R.IX.B.l); }
void Z80::sub_iyh() { M_SUB(R.IY.B.h); }
void Z80::sub_iyl() { M_SUB(R.IY.B.l); }
void Z80::sub_byte(){ M_SUB(Z80_RDMEM_OPCODE()); }

void Z80::xor_xhl() { M_XOR(M_RD_XHL()); }
void Z80::xor_xix() { M_XOR(M_RD_XIX()); }
void Z80::xor_xiy() { M_XOR(M_RD_XIY()); }
void Z80::xor_a()   { R.AF.W.l = Z_FLAG|V_FLAG;}
void Z80::xor_b()   { M_XOR(R.BC.B.h); }
void Z80::xor_c()   { M_XOR(R.BC.B.l); }
void Z80::xor_d()   { M_XOR(R.DE.B.h); }
void Z80::xor_e()   { M_XOR(R.DE.B.l); }
void Z80::xor_h()   { M_XOR(R.HL.B.h); }
void Z80::xor_l()   { M_XOR(R.HL.B.l); }
void Z80::xor_ixh() { M_XOR(R.IX.B.h); }
void Z80::xor_ixl() { M_XOR(R.IX.B.l); }
void Z80::xor_iyh() { M_XOR(R.IY.B.h); }
void Z80::xor_iyl() { M_XOR(R.IY.B.l); }
void Z80::xor_byte(){ M_XOR(Z80_RDMEM_OPCODE()); }

void Z80::no_op()    { --R.PC.W.l; }
void Z80::no_op_xx() { ++R.PC.W.l; }

void Z80::patch() { interface->Z80_Patch(); }



void Z80::dd_cb() {
	unsigned opcode = Z80_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
	R.ICount += cycles_xx_cb[opcode];
	(this->*opcode_dd_cb[opcode])();
	++R.PC.W.l;
}
void Z80::fd_cb() {
	unsigned opcode = Z80_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
	R.ICount += cycles_xx_cb[opcode];
	(this->*opcode_fd_cb[opcode])();
	++R.PC.W.l;
}
void Z80::cb() {
	++R.R;
	unsigned opcode = Z80_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_cb[opcode];
	(this->*opcode_cb[opcode])();
}
void Z80::dd() {
	++R.R;
	unsigned opcode = Z80_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_xx[opcode];
	(this->*opcode_dd[opcode])();
}
void Z80::ed() {
	++R.R;
	unsigned opcode = Z80_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_ed[opcode];
	(this->*opcode_ed[opcode])();
}
void Z80::fd () {
	++R.R;
	unsigned opcode = Z80_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_xx[opcode];
	(this->*opcode_fd[opcode])();
}

void Z80::ei() {
	R.nextIFF1 = true;	// delay one instruction
	R.IFF2 = true;
}

