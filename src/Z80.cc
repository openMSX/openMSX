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
#include <stdlib.h>

#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include "Z80Dasm.h"
#include "Z80.hh"


static byte PTable[512];
static byte ZSTable[512];
static byte ZSPTable[512];
#include "Z80DAA.h"

#ifdef DEBUG
	byte debugmemory[65536];
	char to_print_string[300];
#endif


#define M_RDMEM(A)      Z80_RDMEM(A)
#define M_WRMEM(A,V)    Z80_WRMEM(A,V)
#define M_RDOP(A)       Z80_RDOP(A)
#define M_RDOP_ARG(A)   Z80_RDOP_ARG(A)
#define M_RDSTACK(A)    Z80_RDSTACK(A)
#define M_WRSTACK(A,V)  Z80_WRSTACK(A,V)

#define DoIn(lo,hi)     Z80_In((lo)+(((unsigned)(hi))<<8))
#define DoOut(lo,hi,v)  Z80_Out((lo)+(((unsigned)(hi))<<8),v)


static const byte S_FLAG = 0x80;
static const byte Z_FLAG = 0x40;
static const byte H_FLAG = 0x10;
static const byte V_FLAG = 0x04;
static const byte N_FLAG = 0x02;
static const byte C_FLAG = 0x01;

static inline void M_SKIP_CALL() { R.PC.W.l += 2; }
static inline void M_SKIP_JP()   { R.PC.W.l += 2; }
static inline void M_SKIP_JR()   { R.PC.W.l += 1; }
static inline void M_SKIP_RET()  { }

static inline bool M_C()  { return R.AF.B.l&C_FLAG; }
static inline bool M_NC() { return !M_C(); }
static inline bool M_Z()  { return R.AF.B.l&Z_FLAG; }
static inline bool M_NZ() { return !M_Z(); }
static inline bool M_M()  { return R.AF.B.l&S_FLAG; }
static inline bool M_P()  { return !M_M(); }
static inline bool M_PE() { return R.AF.B.l&V_FLAG; }
static inline bool M_PO() { return !M_PE(); }

/* Get next opcode argument and increment program counter */
static inline byte M_RDMEM_OPCODE () {
	return M_RDOP_ARG(R.PC.W.l++);
}
static inline word M_RDMEM_WORD (word A) {
	return M_RDMEM(A) + (M_RDMEM(((A)+1)&0xFFFF)<<8);
}
static inline void M_WRMEM_WORD (word A,word V) {
	M_WRMEM (A, V&255);
	M_WRMEM (((A)+1)&0xFFFF, V>>8);
}
static inline word M_RDMEM_OPCODE_WORD () {
	return M_RDMEM_OPCODE() + (M_RDMEM_OPCODE()<<8);
}

static inline word M_XIX() { return (R.IX.W.l+(offset)M_RDMEM_OPCODE())&0xFFFF; }
static inline word M_XIY() { return (R.IY.W.l+(offset)M_RDMEM_OPCODE())&0xFFFF; }
static inline byte M_RD_XHL() { return M_RDMEM(R.HL.W.l); }
static inline byte M_RD_XIX() { return M_RDMEM(M_XIX()); }
static inline byte M_RD_XIY() { return M_RDMEM(M_XIY()); }
static inline void M_WR_XIX(byte a) { M_WRMEM(M_XIX(), a); }
static inline void M_WR_XIY(byte a) { M_WRMEM(M_XIY(), a); }

#ifdef X86_ASM
#include "Z80CDx86.h"	// this file is missing (and need updates)
#else
#include "Z80Codes.h"
#endif

typedef void (*opcode_fn) ();

static void adc_a_xhl() { M_ADC(M_RD_XHL()); }
static void adc_a_xix() { M_ADC(M_RD_XIX()); }
static void adc_a_xiy() { M_ADC(M_RD_XIY()); }
static void adc_a_a()   { M_ADC(R.AF.B.h); }
static void adc_a_b()   { M_ADC(R.BC.B.h); }
static void adc_a_c()   { M_ADC(R.BC.B.l); }
static void adc_a_d()   { M_ADC(R.DE.B.h); }
static void adc_a_e()   { M_ADC(R.DE.B.l); }
static void adc_a_h()   { M_ADC(R.HL.B.h); }
static void adc_a_l()   { M_ADC(R.HL.B.l); }
static void adc_a_ixl() { M_ADC(R.IX.B.l); }
static void adc_a_ixh() { M_ADC(R.IX.B.h); }
static void adc_a_iyl() { M_ADC(R.IY.B.l); }
static void adc_a_iyh() { M_ADC(R.IY.B.h); }
static void adc_a_byte(){ M_ADC(M_RDMEM_OPCODE()); }

static void adc_hl_bc() { M_ADCW(R.BC.W.l); }
static void adc_hl_de() { M_ADCW(R.DE.W.l); }
static void adc_hl_hl() { M_ADCW(R.HL.W.l); }
static void adc_hl_sp() { M_ADCW(R.SP.W.l); }

static void add_a_xhl() { M_ADD(M_RD_XHL()); }
static void add_a_xix() { M_ADD(M_RD_XIX()); }
static void add_a_xiy() { M_ADD(M_RD_XIY()); }
static void add_a_a()   { M_ADD(R.AF.B.h); }
static void add_a_b()   { M_ADD(R.BC.B.h); }
static void add_a_c()   { M_ADD(R.BC.B.l); }
static void add_a_d()   { M_ADD(R.DE.B.h); }
static void add_a_e()   { M_ADD(R.DE.B.l); }
static void add_a_h()   { M_ADD(R.HL.B.h); }
static void add_a_l()   { M_ADD(R.HL.B.l); }
static void add_a_ixl() { M_ADD(R.IX.B.l); }
static void add_a_ixh() { M_ADD(R.IX.B.h); }
static void add_a_iyl() { M_ADD(R.IY.B.l); }
static void add_a_iyh() { M_ADD(R.IY.B.h); }
static void add_a_byte(){ M_ADD(M_RDMEM_OPCODE()); }

static void add_hl_bc() { M_ADDW(R.HL.W.l, R.BC.W.l); }
static void add_hl_de() { M_ADDW(R.HL.W.l, R.DE.W.l); }
static void add_hl_hl() { M_ADDW(R.HL.W.l, R.HL.W.l); }
static void add_hl_sp() { M_ADDW(R.HL.W.l, R.SP.W.l); }
static void add_ix_bc() { M_ADDW(R.IX.W.l, R.BC.W.l); }
static void add_ix_de() { M_ADDW(R.IX.W.l, R.DE.W.l); }
static void add_ix_ix() { M_ADDW(R.IX.W.l, R.IX.W.l); }
static void add_ix_sp() { M_ADDW(R.IX.W.l, R.SP.W.l); }
static void add_iy_bc() { M_ADDW(R.IY.W.l, R.BC.W.l); }
static void add_iy_de() { M_ADDW(R.IY.W.l, R.DE.W.l); }
static void add_iy_iy() { M_ADDW(R.IY.W.l, R.IY.W.l); }
static void add_iy_sp() { M_ADDW(R.IY.W.l, R.SP.W.l); }

static void and_xhl() { M_AND(M_RD_XHL()); }
static void and_xix() { M_AND(M_RD_XIX()); }
static void and_xiy() { M_AND(M_RD_XIY()); }
static void and_a()   { R.AF.B.l = ZSPTable[R.AF.B.h]|H_FLAG; }
static void and_b()   { M_AND(R.BC.B.h); }
static void and_c()   { M_AND(R.BC.B.l); }
static void and_d()   { M_AND(R.DE.B.h); }
static void and_e()   { M_AND(R.DE.B.l); }
static void and_h()   { M_AND(R.HL.B.h); }
static void and_l()   { M_AND(R.HL.B.l); }
static void and_ixh() { M_AND(R.IX.B.h); }
static void and_ixl() { M_AND(R.IX.B.l); }
static void and_iyh() { M_AND(R.IY.B.h); }
static void and_iyl() { M_AND(R.IY.B.l); }
static void and_byte(){ M_AND(M_RDMEM_OPCODE()); }

static void bit_0_xhl() { M_BIT(0, M_RD_XHL()); }
static void bit_0_xix() { M_BIT(0, M_RD_XIX()); }
static void bit_0_xiy() { M_BIT(0, M_RD_XIY()); }
static void bit_0_a()   { M_BIT(0, R.AF.B.h); }
static void bit_0_b()   { M_BIT(0, R.BC.B.h); }
static void bit_0_c()   { M_BIT(0, R.BC.B.l); }
static void bit_0_d()   { M_BIT(0, R.DE.B.h); }
static void bit_0_e()   { M_BIT(0, R.DE.B.l); }
static void bit_0_h()   { M_BIT(0, R.HL.B.h); }
static void bit_0_l()   { M_BIT(0, R.HL.B.l); }

static void bit_1_xhl() { M_BIT(1, M_RD_XHL()); }
static void bit_1_xix() { M_BIT(1, M_RD_XIX()); }
static void bit_1_xiy() { M_BIT(1, M_RD_XIY()); }
static void bit_1_a()   { M_BIT(1, R.AF.B.h); }
static void bit_1_b()   { M_BIT(1, R.BC.B.h); }
static void bit_1_c()   { M_BIT(1, R.BC.B.l); }
static void bit_1_d()   { M_BIT(1, R.DE.B.h); }
static void bit_1_e()   { M_BIT(1, R.DE.B.l); }
static void bit_1_h()   { M_BIT(1, R.HL.B.h); }
static void bit_1_l()   { M_BIT(1, R.HL.B.l); }

static void bit_2_xhl() { M_BIT(2, M_RD_XHL()); }
static void bit_2_xix() { M_BIT(2, M_RD_XIX()); }
static void bit_2_xiy() { M_BIT(2, M_RD_XIY()); }
static void bit_2_a()   { M_BIT(2, R.AF.B.h); }
static void bit_2_b()   { M_BIT(2, R.BC.B.h); }
static void bit_2_c()   { M_BIT(2, R.BC.B.l); }
static void bit_2_d()   { M_BIT(2, R.DE.B.h); }
static void bit_2_e()   { M_BIT(2, R.DE.B.l); }
static void bit_2_h()   { M_BIT(2, R.HL.B.h); }
static void bit_2_l()   { M_BIT(2, R.HL.B.l); }

static void bit_3_xhl() { M_BIT(3, M_RD_XHL()); }
static void bit_3_xix() { M_BIT(3, M_RD_XIX()); }
static void bit_3_xiy() { M_BIT(3, M_RD_XIY()); }
static void bit_3_a()   { M_BIT(3, R.AF.B.h); }
static void bit_3_b()   { M_BIT(3, R.BC.B.h); }
static void bit_3_c()   { M_BIT(3, R.BC.B.l); }
static void bit_3_d()   { M_BIT(3, R.DE.B.h); }
static void bit_3_e()   { M_BIT(3, R.DE.B.l); }
static void bit_3_h()   { M_BIT(3, R.HL.B.h); }
static void bit_3_l()   { M_BIT(3, R.HL.B.l); }

static void bit_4_xhl() { M_BIT(4, M_RD_XHL()); }
static void bit_4_xix() { M_BIT(4, M_RD_XIX()); }
static void bit_4_xiy() { M_BIT(4, M_RD_XIY()); }
static void bit_4_a()   { M_BIT(4, R.AF.B.h); }
static void bit_4_b()   { M_BIT(4, R.BC.B.h); }
static void bit_4_c()   { M_BIT(4, R.BC.B.l); }
static void bit_4_d()   { M_BIT(4, R.DE.B.h); }
static void bit_4_e()   { M_BIT(4, R.DE.B.l); }
static void bit_4_h()   { M_BIT(4, R.HL.B.h); }
static void bit_4_l()   { M_BIT(4, R.HL.B.l); }

static void bit_5_xhl() { M_BIT(5, M_RD_XHL()); }
static void bit_5_xix() { M_BIT(5, M_RD_XIX()); }
static void bit_5_xiy() { M_BIT(5, M_RD_XIY()); }
static void bit_5_a()   { M_BIT(5, R.AF.B.h); }
static void bit_5_b()   { M_BIT(5, R.BC.B.h); }
static void bit_5_c()   { M_BIT(5, R.BC.B.l); }
static void bit_5_d()   { M_BIT(5, R.DE.B.h); }
static void bit_5_e()   { M_BIT(5, R.DE.B.l); }
static void bit_5_h()   { M_BIT(5, R.HL.B.h); }
static void bit_5_l()   { M_BIT(5, R.HL.B.l); }

static void bit_6_xhl() { M_BIT(6, M_RD_XHL()); }
static void bit_6_xix() { M_BIT(6, M_RD_XIX()); }
static void bit_6_xiy() { M_BIT(6, M_RD_XIY()); }
static void bit_6_a()   { M_BIT(6, R.AF.B.h); }
static void bit_6_b()   { M_BIT(6, R.BC.B.h); }
static void bit_6_c()   { M_BIT(6, R.BC.B.l); }
static void bit_6_d()   { M_BIT(6, R.DE.B.h); }
static void bit_6_e()   { M_BIT(6, R.DE.B.l); }
static void bit_6_h()   { M_BIT(6, R.HL.B.h); }
static void bit_6_l()   { M_BIT(6, R.HL.B.l); }

static void bit_7_xhl() { M_BIT(7, M_RD_XHL()); }
static void bit_7_xix() { M_BIT(7, M_RD_XIX()); }
static void bit_7_xiy() { M_BIT(7, M_RD_XIY()); }
static void bit_7_a()   { M_BIT(7, R.AF.B.h); }
static void bit_7_b()   { M_BIT(7, R.BC.B.h); }
static void bit_7_c()   { M_BIT(7, R.BC.B.l); }
static void bit_7_d()   { M_BIT(7, R.DE.B.h); }
static void bit_7_e()   { M_BIT(7, R.DE.B.l); }
static void bit_7_h()   { M_BIT(7, R.HL.B.h); }
static void bit_7_l()   { M_BIT(7, R.HL.B.l); }

static void call_c()  { if (M_C())  { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_m()  { if (M_M())  { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_nc() { if (M_NC()) { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_nz() { if (M_NZ()) { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_p()  { if (M_P())  { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_pe() { if (M_PE()) { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_po() { if (M_PO()) { M_CALL(); } else { M_SKIP_CALL(); } }
static void call_z()  { if (M_Z())  { M_CALL(); } else { M_SKIP_CALL(); } }
static void call()                  { M_CALL(); }

static void ccf() { R.AF.B.l = ((R.AF.B.l&0xED)|((R.AF.B.l&1)<<4))^1; }

static void cp_xhl() { M_CP(M_RD_XHL()); }
static void cp_xix() { M_CP(M_RD_XIX()); }
static void cp_xiy() { M_CP(M_RD_XIY()); }
static void cp_a()   { M_CP(R.AF.B.h); }
static void cp_b()   { M_CP(R.BC.B.h); }
static void cp_c()   { M_CP(R.BC.B.l); }
static void cp_d()   { M_CP(R.DE.B.h); }
static void cp_e()   { M_CP(R.DE.B.l); }
static void cp_h()   { M_CP(R.HL.B.h); }
static void cp_l()   { M_CP(R.HL.B.l); }
static void cp_ixh() { M_CP(R.IX.B.h); }
static void cp_ixl() { M_CP(R.IX.B.l); }
static void cp_iyh() { M_CP(R.IY.B.h); }
static void cp_iyl() { M_CP(R.IY.B.l); }
static void cp_byte(){ M_CP(M_RDMEM_OPCODE()); }

static void cpd() {
	byte i = M_RDMEM(R.HL.W.l--);
	byte j = R.AF.B.h-i;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[j]|
	           ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}
static void cpdr() {
	cpd ();
	if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

static void cpi() {
	byte i = M_RDMEM(R.HL.W.l++);
	byte j = R.AF.B.h-i;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[j]|
	           ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}

static void cpir() {
	cpi ();
	if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

static void cpl() { R.AF.B.h^=0xFF; R.AF.B.l|=(H_FLAG|N_FLAG); }

static void daa() {
	int i = R.AF.B.h;
	if (R.AF.B.l&C_FLAG) i|=256;
	if (R.AF.B.l&H_FLAG) i|=512;
	if (R.AF.B.l&N_FLAG) i|=1024;
	R.AF.W.l = DAATable[i];
}

static void dec_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_DEC(i);
	M_WRMEM(R.HL.W.l, i);
}
static void dec_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_DEC(i);
	M_WRMEM(j, i);
}
static void dec_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_DEC(i);
	M_WRMEM(j, i);
}
static void dec_a()   { M_DEC(R.AF.B.h); }
static void dec_b()   { M_DEC(R.BC.B.h); }
static void dec_c()   { M_DEC(R.BC.B.l); }
static void dec_d()   { M_DEC(R.DE.B.h); }
static void dec_e()   { M_DEC(R.DE.B.l); }
static void dec_h()   { M_DEC(R.HL.B.h); }
static void dec_l()   { M_DEC(R.HL.B.l); }
static void dec_ixh() { M_DEC(R.IX.B.h); }
static void dec_ixl() { M_DEC(R.IX.B.l); }
static void dec_iyh() { M_DEC(R.IY.B.h); }
static void dec_iyl() { M_DEC(R.IY.B.l); }

static void dec_bc() { --R.BC.W.l; }
static void dec_de() { --R.DE.W.l; }
static void dec_hl() { --R.HL.W.l; }
static void dec_ix() { --R.IX.W.l; }
static void dec_iy() { --R.IY.W.l; }
static void dec_sp() { --R.SP.W.l; }

static void di() { R.IFF1=R.IFF2=0; }

static void djnz() { if (--R.BC.B.h) { M_JR(); } else { M_SKIP_JR(); } }

static void ex_xsp_hl() {
	int i = M_RDMEM_WORD(R.SP.W.l);
	M_WRMEM_WORD(R.SP.W.l, R.HL.W.l);
	R.HL.W.l = i;
}
static void ex_xsp_ix() {
	int i = M_RDMEM_WORD(R.SP.W.l);
	M_WRMEM_WORD(R.SP.W.l, R.IX.W.l);
	R.IX.W.l = i;
}
static void ex_xsp_iy()
{
	int i = M_RDMEM_WORD(R.SP.W.l);
	M_WRMEM_WORD(R.SP.W.l, R.IY.W.l);
	R.IY.W.l = i;
}
static void ex_af_af() {
	int i = R.AF.W.l;
	R.AF.W.l = R.AF2.W.l;
	R.AF2.W.l = i;
}
static void ex_de_hl() {
	int i = R.DE.W.l;
	R.DE.W.l = R.HL.W.l;
	R.HL.W.l = i;
}
static void exx() {
	int i;
	i = R.BC.W.l; R.BC.W.l = R.BC2.W.l; R.BC2.W.l = i;
	i = R.DE.W.l; R.DE.W.l = R.DE2.W.l; R.DE2.W.l = i;
	i = R.HL.W.l; R.HL.W.l = R.HL2.W.l; R.HL2.W.l = i;
}

static void halt() {
	--R.PC.W.l;
	R.HALT=1;
	if (R.ICount>0) R.ICount=0;
	//TODO: Set CurrentCPUTime to targetCPUTime
}

static void im_0() { R.IM = 0; }
static void im_1() { R.IM = 1; }
static void im_2() { R.IM = 2; }

static void in_a_c() { M_IN(R.AF.B.h); }
static void in_b_c() { M_IN(R.BC.B.h); }
static void in_c_c() { M_IN(R.BC.B.l); }
static void in_d_c() { M_IN(R.DE.B.h); }
static void in_e_c() { M_IN(R.DE.B.l); }
static void in_h_c() { M_IN(R.HL.B.h); }
static void in_l_c() { M_IN(R.HL.B.l); }
static void in_0_c() { byte i; M_IN(i); }
static void in_a_byte() { R.AF.B.h = DoIn(M_RDMEM_OPCODE(), R.AF.B.h); }

static void inc_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_INC(i);
	M_WRMEM(R.HL.W.l, i);
}
static void inc_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_INC(i);
	M_WRMEM(j, i);
}
static void inc_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_INC(i);
	M_WRMEM(j, i);
}
static void inc_a()   { M_INC(R.AF.B.h); }
static void inc_b()   { M_INC(R.BC.B.h); }
static void inc_c()   { M_INC(R.BC.B.l); }
static void inc_d()   { M_INC(R.DE.B.h); }
static void inc_e()   { M_INC(R.DE.B.l); }
static void inc_h()   { M_INC(R.HL.B.h); }
static void inc_l()   { M_INC(R.HL.B.l); }
static void inc_ixh() { M_INC(R.IX.B.h); }
static void inc_ixl() { M_INC(R.IX.B.l); }
static void inc_iyh() { M_INC(R.IY.B.h); }
static void inc_iyl() { M_INC(R.IY.B.l); }

static void inc_bc() { ++R.BC.W.l; }
static void inc_de() { ++R.DE.W.l; }
static void inc_hl() { ++R.HL.W.l; }
static void inc_ix() { ++R.IX.W.l; }
static void inc_iy() { ++R.IY.W.l; }
static void inc_sp() { ++R.SP.W.l; }

static void ind() {
	--R.BC.B.h;
	M_WRMEM(R.HL.W.l, DoIn(R.BC.B.l, R.BC.B.h));
	--R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (N_FLAG|Z_FLAG);
}
static void indr() {
	ind ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

static void ini() {
	--R.BC.B.h;
	M_WRMEM(R.HL.W.l, DoIn(R.BC.B.l, R.BC.B.h));
	++R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (N_FLAG|Z_FLAG);
}
static void inir() {
	ini ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

static void jp_hl() { R.PC.W.l = R.HL.W.l; }
static void jp_ix() { R.PC.W.l = R.IX.W.l; }
static void jp_iy() { R.PC.W.l = R.IY.W.l; }
static void jp()                  { M_JP(); }
static void jp_c()  { if (M_C())  { M_JP(); } else { M_SKIP_JP(); } }
static void jp_m()  { if (M_M())  { M_JP(); } else { M_SKIP_JP(); } }
static void jp_nc() { if (M_NC()) { M_JP(); } else { M_SKIP_JP(); } }
static void jp_nz() { if (M_NZ()) { M_JP(); } else { M_SKIP_JP(); } }
static void jp_p()  { if (M_P())  { M_JP(); } else { M_SKIP_JP(); } }
static void jp_pe() { if (M_PE()) { M_JP(); } else { M_SKIP_JP(); } }
static void jp_po() { if (M_PO()) { M_JP(); } else { M_SKIP_JP(); } }
static void jp_z()  { if (M_Z())  { M_JP(); } else { M_SKIP_JP(); } }

static void jr()                { M_JR(); }
static void jr_c()  { if (M_C())  { M_JR(); } else { M_SKIP_JR(); } }
static void jr_nc() { if (M_NC()) { M_JR(); } else { M_SKIP_JR(); } }
static void jr_nz() { if (M_NZ()) { M_JR(); } else { M_SKIP_JR(); } }
static void jr_z()  { if (M_Z())  { M_JR(); } else { M_SKIP_JR(); } }

static void ld_xbc_a()   { M_WRMEM(R.BC.W.l, R.AF.B.h); }
static void ld_xde_a()   { M_WRMEM(R.DE.W.l, R.AF.B.h); }
static void ld_xhl_a()   { M_WRMEM(R.HL.W.l, R.AF.B.h); }
static void ld_xhl_b()   { M_WRMEM(R.HL.W.l, R.BC.B.h); }
static void ld_xhl_c()   { M_WRMEM(R.HL.W.l, R.BC.B.l); }
static void ld_xhl_d()   { M_WRMEM(R.HL.W.l, R.DE.B.h); }
static void ld_xhl_e()   { M_WRMEM(R.HL.W.l, R.DE.B.l); }
static void ld_xhl_h()   { M_WRMEM(R.HL.W.l, R.HL.B.h); }
static void ld_xhl_l()   { M_WRMEM(R.HL.W.l, R.HL.B.l); }
static void ld_xhl_byte(){ M_WRMEM(R.HL.W.l, M_RDMEM_OPCODE()); }
static void ld_xix_a()   { M_WR_XIX(R.AF.B.h); }
static void ld_xix_b()   { M_WR_XIX(R.BC.B.h); }
static void ld_xix_c()   { M_WR_XIX(R.BC.B.l); }
static void ld_xix_d()   { M_WR_XIX(R.DE.B.h); }
static void ld_xix_e()   { M_WR_XIX(R.DE.B.l); }
static void ld_xix_h()   { M_WR_XIX(R.HL.B.h); }
static void ld_xix_l()   { M_WR_XIX(R.HL.B.l); }
static void ld_xix_byte(){ M_WRMEM(M_XIX(), M_RDMEM_OPCODE()); }
static void ld_xiy_a()   { M_WR_XIY(R.AF.B.h); }
static void ld_xiy_b()   { M_WR_XIY(R.BC.B.h); }
static void ld_xiy_c()   { M_WR_XIY(R.BC.B.l); }
static void ld_xiy_d()   { M_WR_XIY(R.DE.B.h); }
static void ld_xiy_e()   { M_WR_XIY(R.DE.B.l); }
static void ld_xiy_h()   { M_WR_XIY(R.HL.B.h); }
static void ld_xiy_l()   { M_WR_XIY(R.HL.B.l); }
static void ld_xiy_byte(){ M_WRMEM(M_XIY(), M_RDMEM_OPCODE()); }
static void ld_xbyte_a() { M_WRMEM(M_RDMEM_OPCODE_WORD(), R.AF.B.h); }
static void ld_xword_bc(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.BC.W.l); }
static void ld_xword_de(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.DE.W.l); }
static void ld_xword_hl(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.HL.W.l); }
static void ld_xword_ix(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.IX.W.l); }
static void ld_xword_iy(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.IY.W.l); }
static void ld_xword_sp(){ M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(), R.SP.W.l); }
static void ld_a_xbc()   { R.AF.B.h = M_RDMEM(R.BC.W.l); }
static void ld_a_xde()   { R.AF.B.h = M_RDMEM(R.DE.W.l); }
static void ld_a_xhl()   { R.AF.B.h = M_RD_XHL(); }
static void ld_a_xix()   { R.AF.B.h = M_RD_XIX(); }
static void ld_a_xiy()   { R.AF.B.h = M_RD_XIY(); }
static void ld_a_xbyte() { R.AF.B.h = M_RDMEM(M_RDMEM_OPCODE_WORD()); }
static void ld_a_byte()  { R.AF.B.h = M_RDMEM_OPCODE(); }
static void ld_b_byte()  { R.BC.B.h = M_RDMEM_OPCODE(); }
static void ld_c_byte()  { R.BC.B.l = M_RDMEM_OPCODE(); }
static void ld_d_byte()  { R.DE.B.h = M_RDMEM_OPCODE(); }
static void ld_e_byte()  { R.DE.B.l = M_RDMEM_OPCODE(); }
static void ld_h_byte()  { R.HL.B.h = M_RDMEM_OPCODE(); }
static void ld_l_byte()  { R.HL.B.l = M_RDMEM_OPCODE(); }
static void ld_ixh_byte(){ R.IX.B.h = M_RDMEM_OPCODE(); }
static void ld_ixl_byte(){ R.IX.B.l = M_RDMEM_OPCODE(); }
static void ld_iyh_byte(){ R.IY.B.h = M_RDMEM_OPCODE(); }
static void ld_iyl_byte(){ R.IY.B.l = M_RDMEM_OPCODE(); }
static void ld_b_xhl()   { R.BC.B.h = M_RD_XHL(); }
static void ld_c_xhl()   { R.BC.B.l = M_RD_XHL(); }
static void ld_d_xhl()   { R.DE.B.h = M_RD_XHL(); }
static void ld_e_xhl()   { R.DE.B.l = M_RD_XHL(); }
static void ld_h_xhl()   { R.HL.B.h = M_RD_XHL(); }
static void ld_l_xhl()   { R.HL.B.l = M_RD_XHL(); }
static void ld_b_xix()   { R.BC.B.h = M_RD_XIX(); }
static void ld_c_xix()   { R.BC.B.l = M_RD_XIX(); }
static void ld_d_xix()   { R.DE.B.h = M_RD_XIX(); }
static void ld_e_xix()   { R.DE.B.l = M_RD_XIX(); }
static void ld_h_xix()   { R.HL.B.h = M_RD_XIX(); }
static void ld_l_xix()   { R.HL.B.l = M_RD_XIX(); }
static void ld_b_xiy()   { R.BC.B.h = M_RD_XIY(); }
static void ld_c_xiy()   { R.BC.B.l = M_RD_XIY(); }
static void ld_d_xiy()   { R.DE.B.h = M_RD_XIY(); }
static void ld_e_xiy()   { R.DE.B.l = M_RD_XIY(); }
static void ld_h_xiy()   { R.HL.B.h = M_RD_XIY(); }
static void ld_l_xiy()   { R.HL.B.l = M_RD_XIY(); }
static void ld_a_a()     { }
static void ld_a_b()     { R.AF.B.h = R.BC.B.h; }
static void ld_a_c()     { R.AF.B.h = R.BC.B.l; }
static void ld_a_d()     { R.AF.B.h = R.DE.B.h; }
static void ld_a_e()     { R.AF.B.h = R.DE.B.l; }
static void ld_a_h()     { R.AF.B.h = R.HL.B.h; }
static void ld_a_l()     { R.AF.B.h = R.HL.B.l; }
static void ld_a_ixh()   { R.AF.B.h = R.IX.B.h; }
static void ld_a_ixl()   { R.AF.B.h = R.IX.B.l; }
static void ld_a_iyh()   { R.AF.B.h = R.IY.B.h; }
static void ld_a_iyl()   { R.AF.B.h = R.IY.B.l; }
static void ld_b_b()     { }
static void ld_b_a()     { R.BC.B.h = R.AF.B.h; }
static void ld_b_c()     { R.BC.B.h = R.BC.B.l; }
static void ld_b_d()     { R.BC.B.h = R.DE.B.h; }
static void ld_b_e()     { R.BC.B.h = R.DE.B.l; }
static void ld_b_h()     { R.BC.B.h = R.HL.B.h; }
static void ld_b_l()     { R.BC.B.h = R.HL.B.l; }
static void ld_b_ixh()   { R.BC.B.h = R.IX.B.h; }
static void ld_b_ixl()   { R.BC.B.h = R.IX.B.l; }
static void ld_b_iyh()   { R.BC.B.h = R.IY.B.h; }
static void ld_b_iyl()   { R.BC.B.h = R.IY.B.l; }
static void ld_c_c()     { }
static void ld_c_a()     { R.BC.B.l = R.AF.B.h; }
static void ld_c_b()     { R.BC.B.l = R.BC.B.h; }
static void ld_c_d()     { R.BC.B.l = R.DE.B.h; }
static void ld_c_e()     { R.BC.B.l = R.DE.B.l; }
static void ld_c_h()     { R.BC.B.l = R.HL.B.h; }
static void ld_c_l()     { R.BC.B.l = R.HL.B.l; }
static void ld_c_ixh()   { R.BC.B.l = R.IX.B.h; }
static void ld_c_ixl()   { R.BC.B.l = R.IX.B.l; }
static void ld_c_iyh()   { R.BC.B.l = R.IY.B.h; }
static void ld_c_iyl()   { R.BC.B.l = R.IY.B.l; }
static void ld_d_d()     { }
static void ld_d_a()     { R.DE.B.h = R.AF.B.h; }
static void ld_d_c()     { R.DE.B.h = R.BC.B.l; }
static void ld_d_b()     { R.DE.B.h = R.BC.B.h; }
static void ld_d_e()     { R.DE.B.h = R.DE.B.l; }
static void ld_d_h()     { R.DE.B.h = R.HL.B.h; }
static void ld_d_l()     { R.DE.B.h = R.HL.B.l; }
static void ld_d_ixh()   { R.DE.B.h = R.IX.B.h; }
static void ld_d_ixl()   { R.DE.B.h = R.IX.B.l; }
static void ld_d_iyh()   { R.DE.B.h = R.IY.B.h; }
static void ld_d_iyl()   { R.DE.B.h = R.IY.B.l; }
static void ld_e_e()     { }
static void ld_e_a()     { R.DE.B.l = R.AF.B.h; }
static void ld_e_c()     { R.DE.B.l = R.BC.B.l; }
static void ld_e_b()     { R.DE.B.l = R.BC.B.h; }
static void ld_e_d()     { R.DE.B.l = R.DE.B.h; }
static void ld_e_h()     { R.DE.B.l = R.HL.B.h; }
static void ld_e_l()     { R.DE.B.l = R.HL.B.l; }
static void ld_e_ixh()   { R.DE.B.l = R.IX.B.h; }
static void ld_e_ixl()   { R.DE.B.l = R.IX.B.l; }
static void ld_e_iyh()   { R.DE.B.l = R.IY.B.h; }
static void ld_e_iyl()   { R.DE.B.l = R.IY.B.l; }
static void ld_h_h()     { }
static void ld_h_a()     { R.HL.B.h = R.AF.B.h; }
static void ld_h_c()     { R.HL.B.h = R.BC.B.l; }
static void ld_h_b()     { R.HL.B.h = R.BC.B.h; }
static void ld_h_e()     { R.HL.B.h = R.DE.B.l; }
static void ld_h_d()     { R.HL.B.h = R.DE.B.h; }
static void ld_h_l()     { R.HL.B.h = R.HL.B.l; }
static void ld_l_l()     { }
static void ld_l_a()     { R.HL.B.l = R.AF.B.h; }
static void ld_l_c()     { R.HL.B.l = R.BC.B.l; }
static void ld_l_b()     { R.HL.B.l = R.BC.B.h; }
static void ld_l_e()     { R.HL.B.l = R.DE.B.l; }
static void ld_l_d()     { R.HL.B.l = R.DE.B.h; }
static void ld_l_h()     { R.HL.B.l = R.HL.B.h; }
static void ld_ixh_a()   { R.IX.B.h = R.AF.B.h; }
static void ld_ixh_b()   { R.IX.B.h = R.BC.B.h; }
static void ld_ixh_c()   { R.IX.B.h = R.BC.B.l; }
static void ld_ixh_d()   { R.IX.B.h = R.DE.B.h; }
static void ld_ixh_e()   { R.IX.B.h = R.DE.B.l; }
static void ld_ixh_ixh() { }
static void ld_ixh_ixl() { R.IX.B.h = R.IX.B.l; }
static void ld_ixl_a()   { R.IX.B.l = R.AF.B.h; }
static void ld_ixl_b()   { R.IX.B.l = R.BC.B.h; }
static void ld_ixl_c()   { R.IX.B.l = R.BC.B.l; }
static void ld_ixl_d()   { R.IX.B.l = R.DE.B.h; }
static void ld_ixl_e()   { R.IX.B.l = R.DE.B.l; }
static void ld_ixl_ixh() { R.IX.B.l = R.IX.B.h; }
static void ld_ixl_ixl() { }
static void ld_iyh_a()   { R.IY.B.h = R.AF.B.h; }
static void ld_iyh_b()   { R.IY.B.h = R.BC.B.h; }
static void ld_iyh_c()   { R.IY.B.h = R.BC.B.l; }
static void ld_iyh_d()   { R.IY.B.h = R.DE.B.h; }
static void ld_iyh_e()   { R.IY.B.h = R.DE.B.l; }
static void ld_iyh_iyh() { }
static void ld_iyh_iyl() { R.IY.B.h = R.IY.B.l; }
static void ld_iyl_a()   { R.IY.B.l = R.AF.B.h; }
static void ld_iyl_b()   { R.IY.B.l = R.BC.B.h; }
static void ld_iyl_c()   { R.IY.B.l = R.BC.B.l; }
static void ld_iyl_d()   { R.IY.B.l = R.DE.B.h; }
static void ld_iyl_e()   { R.IY.B.l = R.DE.B.l; }
static void ld_iyl_iyh() { R.IY.B.l = R.IY.B.h; }
static void ld_iyl_iyl() { }
static void ld_bc_xword(){ R.BC.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_bc_word() { R.BC.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_de_xword(){ R.DE.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_de_word() { R.DE.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_hl_xword(){ R.HL.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_hl_word() { R.HL.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_ix_xword(){ R.IX.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_ix_word() { R.IX.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_iy_xword(){ R.IY.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_iy_word() { R.IY.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_sp_xword(){ R.SP.W.l = M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_sp_word() { R.SP.W.l = M_RDMEM_OPCODE_WORD(); }
static void ld_sp_hl()   { R.SP.W.l = R.HL.W.l; }
static void ld_sp_ix()   { R.SP.W.l = R.IX.W.l; }
static void ld_sp_iy()   { R.SP.W.l = R.IY.W.l; }
static void ld_a_i() {
	R.AF.B.h = R.I;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[R.I]|(R.IFF2<<2);
}
static void ld_i_a()     { R.I = R.AF.B.h; }
static void ld_a_r() {
	R.AF.B.h=(R.R&127)|(R.R2&128);
	R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.AF.B.h]|(R.IFF2<<2);
}
static void ld_r_a()     { R.R = R.R2 = R.AF.B.h; }

static void ldd() {
	M_WRMEM(R.DE.W.l,M_RDMEM(R.HL.W.l));
	--R.DE.W.l;
	--R.HL.W.l;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&0xE9)|(R.BC.W.l ? V_FLAG : 0);
}
static void lddr() {
	ldd ();
	if (R.BC.W.l) { R.ICount += 5; R.PC.W.l -= 2; }
}
static void ldi() {
	M_WRMEM(R.DE.W.l,M_RDMEM(R.HL.W.l));
	++R.DE.W.l;
	++R.HL.W.l;
	--R.BC.W.l;
	R.AF.B.l = (R.AF.B.l&0xE9)|(R.BC.W.l ? V_FLAG : 0);
}
static void ldir() {
	ldi ();
	if (R.BC.W.l) { R.ICount += 5; R.PC.W.l -= 2; }
}

static void neg() {
	 byte i = R.AF.B.h;
	 R.AF.B.h = 0;
	 M_SUB(i);
}

static void nop() { };

static void or_xhl() { M_OR(M_RD_XHL()); }
static void or_xix() { M_OR(M_RD_XIX()); }
static void or_xiy() { M_OR(M_RD_XIY()); }
static void or_a()   { R.AF.B.l = ZSPTable[R.AF.B.h]; }
static void or_b()   { M_OR(R.BC.B.h); }
static void or_c()   { M_OR(R.BC.B.l); }
static void or_d()   { M_OR(R.DE.B.h); }
static void or_e()   { M_OR(R.DE.B.l); }
static void or_h()   { M_OR(R.HL.B.h); }
static void or_l()   { M_OR(R.HL.B.l); }
static void or_ixh() { M_OR(R.IX.B.h); }
static void or_ixl() { M_OR(R.IX.B.l); }
static void or_iyh() { M_OR(R.IY.B.h); }
static void or_iyl() { M_OR(R.IY.B.l); }
static void or_byte(){ M_OR(M_RDMEM_OPCODE()); }

static void outd() {
	--R.BC.B.h;
	DoOut (R.BC.B.l, R.BC.B.h, M_RDMEM(R.HL.W.l));
	--R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (Z_FLAG|N_FLAG);
}
static void otdr() {
	outd ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}
static void outi() {
	--R.BC.B.h;
	DoOut (R.BC.B.l,R.BC.B.h,M_RDMEM(R.HL.W.l));
	++R.HL.W.l;
	R.AF.B.l = (R.BC.B.h) ? N_FLAG : (Z_FLAG|N_FLAG);
}
static void otir() {
	outi ();
	if (R.BC.B.h) { R.ICount += 5; R.PC.W.l -= 2; }
}

static void out_c_a()   { DoOut(R.BC.B.l, R.BC.B.h, R.AF.B.h); }
static void out_c_b()   { DoOut(R.BC.B.l, R.BC.B.h, R.BC.B.h); }
static void out_c_c()   { DoOut(R.BC.B.l, R.BC.B.h, R.BC.B.l); }
static void out_c_d()   { DoOut(R.BC.B.l, R.BC.B.h, R.DE.B.h); }
static void out_c_e()   { DoOut(R.BC.B.l, R.BC.B.h, R.DE.B.l); }
static void out_c_h()   { DoOut(R.BC.B.l, R.BC.B.h, R.HL.B.h); }
static void out_c_l()   { DoOut(R.BC.B.l, R.BC.B.h, R.HL.B.l); }
static void out_c_0()   { DoOut(R.BC.B.l, R.BC.B.h, 0); }
static void out_byte_a(){ DoOut(M_RDMEM_OPCODE(), R.AF.B.h, R.AF.B.h); }

static void pop_af() { M_POP(R.AF.W.l); }
static void pop_bc() { M_POP(R.BC.W.l); }
static void pop_de() { M_POP(R.DE.W.l); }
static void pop_hl() { M_POP(R.HL.W.l); }
static void pop_ix() { M_POP(R.IX.W.l); }
static void pop_iy() { M_POP(R.IY.W.l); }

static void push_af() { M_PUSH(R.AF.W.l); }
static void push_bc() { M_PUSH(R.BC.W.l); }
static void push_de() { M_PUSH(R.DE.W.l); }
static void push_hl() { M_PUSH(R.HL.W.l); }
static void push_ix() { M_PUSH(R.IX.W.l); }
static void push_iy() { M_PUSH(R.IY.W.l); }

static void res_0_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(0, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_0_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(0, i);
	M_WRMEM(j, i);
}
static void res_0_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(0, i);
	M_WRMEM(j, i);
}
static void res_0_a() { M_RES(0, R.AF.B.h); };
static void res_0_b() { M_RES(0, R.BC.B.h); };
static void res_0_c() { M_RES(0, R.BC.B.l); };
static void res_0_d() { M_RES(0, R.DE.B.h); };
static void res_0_e() { M_RES(0, R.DE.B.l); };
static void res_0_h() { M_RES(0, R.HL.B.h); };
static void res_0_l() { M_RES(0, R.HL.B.l); };

static void res_1_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(1, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_1_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(1, i);
	M_WRMEM(j, i);
}
static void res_1_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(1, i);
	M_WRMEM(j, i);
}
static void res_1_a() { M_RES(1, R.AF.B.h); };
static void res_1_b() { M_RES(1, R.BC.B.h); };
static void res_1_c() { M_RES(1, R.BC.B.l); };
static void res_1_d() { M_RES(1, R.DE.B.h); };
static void res_1_e() { M_RES(1, R.DE.B.l); };
static void res_1_h() { M_RES(1, R.HL.B.h); };
static void res_1_l() { M_RES(1, R.HL.B.l); };

static void res_2_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(2, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_2_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(2, i);
	M_WRMEM(j, i);
}
static void res_2_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(2, i);
	M_WRMEM(j, i);
}
static void res_2_a() { M_RES(2, R.AF.B.h); };
static void res_2_b() { M_RES(2, R.BC.B.h); };
static void res_2_c() { M_RES(2, R.BC.B.l); };
static void res_2_d() { M_RES(2, R.DE.B.h); };
static void res_2_e() { M_RES(2, R.DE.B.l); };
static void res_2_h() { M_RES(2, R.HL.B.h); };
static void res_2_l() { M_RES(2, R.HL.B.l); };

static void res_3_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(3, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_3_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(3, i);
	M_WRMEM(j, i);
}
static void res_3_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(3, i);
	M_WRMEM(j, i);
}
static void res_3_a() { M_RES(3, R.AF.B.h); };
static void res_3_b() { M_RES(3, R.BC.B.h); };
static void res_3_c() { M_RES(3, R.BC.B.l); };
static void res_3_d() { M_RES(3, R.DE.B.h); };
static void res_3_e() { M_RES(3, R.DE.B.l); };
static void res_3_h() { M_RES(3, R.HL.B.h); };
static void res_3_l() { M_RES(3, R.HL.B.l); };

static void res_4_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(4, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_4_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(4, i);
	M_WRMEM(j, i);
}
static void res_4_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(4, i);
	M_WRMEM(j, i);
}
static void res_4_a() { M_RES(4, R.AF.B.h); };
static void res_4_b() { M_RES(4, R.BC.B.h); };
static void res_4_c() { M_RES(4, R.BC.B.l); };
static void res_4_d() { M_RES(4, R.DE.B.h); };
static void res_4_e() { M_RES(4, R.DE.B.l); };
static void res_4_h() { M_RES(4, R.HL.B.h); };
static void res_4_l() { M_RES(4, R.HL.B.l); };

static void res_5_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(5, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_5_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(5, i);
	M_WRMEM(j, i);
}
static void res_5_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(5, i);
	M_WRMEM(j, i);
}
static void res_5_a() { M_RES(5, R.AF.B.h); };
static void res_5_b() { M_RES(5, R.BC.B.h); };
static void res_5_c() { M_RES(5, R.BC.B.l); };
static void res_5_d() { M_RES(5, R.DE.B.h); };
static void res_5_e() { M_RES(5, R.DE.B.l); };
static void res_5_h() { M_RES(5, R.HL.B.h); };
static void res_5_l() { M_RES(5, R.HL.B.l); };

static void res_6_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(6, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_6_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(6, i);
	M_WRMEM(j, i);
}
static void res_6_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(6, i);
	M_WRMEM(j, i);
}
static void res_6_a() { M_RES(6, R.AF.B.h); };
static void res_6_b() { M_RES(6, R.BC.B.h); };
static void res_6_c() { M_RES(6, R.BC.B.l); };
static void res_6_d() { M_RES(6, R.DE.B.h); };
static void res_6_e() { M_RES(6, R.DE.B.l); };
static void res_6_h() { M_RES(6, R.HL.B.h); };
static void res_6_l() { M_RES(6, R.HL.B.l); };

static void res_7_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RES(7, i);
	M_WRMEM(R.HL.W.l, i);
}
static void res_7_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RES(7, i);
	M_WRMEM(j, i);
}
static void res_7_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RES(7, i);
	M_WRMEM(j, i);
}
static void res_7_a() { M_RES(7, R.AF.B.h); };
static void res_7_b() { M_RES(7, R.BC.B.h); };
static void res_7_c() { M_RES(7, R.BC.B.l); };
static void res_7_d() { M_RES(7, R.DE.B.h); };
static void res_7_e() { M_RES(7, R.DE.B.l); };
static void res_7_h() { M_RES(7, R.HL.B.h); };
static void res_7_l() { M_RES(7, R.HL.B.l); };

static void ret()                { M_RET(); }
static void ret_c()  { if (M_C())  { M_RET(); } else { M_SKIP_RET(); } }
static void ret_m()  { if (M_M())  { M_RET(); } else { M_SKIP_RET(); } }
static void ret_nc() { if (M_NC()) { M_RET(); } else { M_SKIP_RET(); } }
static void ret_nz() { if (M_NZ()) { M_RET(); } else { M_SKIP_RET(); } }
static void ret_p()  { if (M_P())  { M_RET(); } else { M_SKIP_RET(); } }
static void ret_pe() { if (M_PE()) { M_RET(); } else { M_SKIP_RET(); } }
static void ret_po() { if (M_PO()) { M_RET(); } else { M_SKIP_RET(); } }
static void ret_z()  { if (M_Z())  { M_RET(); } else { M_SKIP_RET(); } }

//static void reti() { Z80_Reti(); M_RET(); }
//static void retn() { R.IFF1=R.IFF2; Z80_Retn(); M_RET(); }
static void reti() { M_RET(); }
static void retn() { R.IFF1 = R.IFF2; M_RET(); }

static void rl_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RL(i);
	M_WRMEM(R.HL.W.l, i);
}
static void rl_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RL(i);
	M_WRMEM(j, i);
}
static void rl_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RL(i);
	M_WRMEM(j, i);
}
static void rl_a() { M_RL(R.AF.B.h); }
static void rl_b() { M_RL(R.BC.B.h); }
static void rl_c() { M_RL(R.BC.B.l); }
static void rl_d() { M_RL(R.DE.B.h); }
static void rl_e() { M_RL(R.DE.B.l); }
static void rl_h() { M_RL(R.HL.B.h); }
static void rl_l() { M_RL(R.HL.B.l); }
static void rla()  { M_RLA(); }

static void rlc_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RLC(i);
	M_WRMEM(R.HL.W.l, i);
}
static void rlc_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RLC(i);
	M_WRMEM(j, i);
}
static void rlc_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RLC(i);
	M_WRMEM(j, i);
}
static void rlc_a() { M_RLC(R.AF.B.h); }
static void rlc_b() { M_RLC(R.BC.B.h); }
static void rlc_c() { M_RLC(R.BC.B.l); }
static void rlc_d() { M_RLC(R.DE.B.h); }
static void rlc_e() { M_RLC(R.DE.B.l); }
static void rlc_h() { M_RLC(R.HL.B.h); }
static void rlc_l() { M_RLC(R.HL.B.l); }
static void rlca()  { M_RLCA(); }

static void rld() {
	byte i = M_RDMEM(R.HL.W.l);
	M_WRMEM(R.HL.W.l, (i<<4)|(R.AF.B.h&0x0F));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i>>4);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rr_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RR(i);
	M_WRMEM(R.HL.W.l, i);
}
static void rr_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RR(i);
	M_WRMEM(j, i);
}
static void rr_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RR(i);
	M_WRMEM(j, i);
}
static void rr_a() { M_RR(R.AF.B.h); }
static void rr_b() { M_RR(R.BC.B.h); }
static void rr_c() { M_RR(R.BC.B.l); }
static void rr_d() { M_RR(R.DE.B.h); }
static void rr_e() { M_RR(R.DE.B.l); }
static void rr_h() { M_RR(R.HL.B.h); }
static void rr_l() { M_RR(R.HL.B.l); }
static void rra()  { M_RRA(); }

static void rrc_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_RRC(i);
	M_WRMEM(R.HL.W.l, i);
}
static void rrc_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_RRC(i);
	M_WRMEM(j, i);
}
static void rrc_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_RRC(i);
	M_WRMEM(j, i);
}
static void rrc_a() { M_RRC(R.AF.B.h); }
static void rrc_b() { M_RRC(R.BC.B.h); }
static void rrc_c() { M_RRC(R.BC.B.l); }
static void rrc_d() { M_RRC(R.DE.B.h); }
static void rrc_e() { M_RRC(R.DE.B.l); }
static void rrc_h() { M_RRC(R.HL.B.h); }
static void rrc_l() { M_RRC(R.HL.B.l); }
static void rrca()  { M_RRCA(); }

static void rrd() {
	byte i = M_RDMEM(R.HL.W.l);
	M_WRMEM(R.HL.W.l, (i>>4)|(R.AF.B.h<<4));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i&0x0F);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rst_00() { M_RST(0x00); }
static void rst_08() { M_RST(0x08); }
static void rst_10() { M_RST(0x10); }
static void rst_18() { M_RST(0x18); }
static void rst_20() { M_RST(0x20); }
static void rst_28() { M_RST(0x28); }
static void rst_30() { M_RST(0x30); }
static void rst_38() { M_RST(0x38); }

static void sbc_a_byte(){ M_SBC(M_RDMEM_OPCODE()); }
static void sbc_a_xhl() { M_SBC(M_RD_XHL()); }
static void sbc_a_xix() { M_SBC(M_RD_XIX()); }
static void sbc_a_xiy() { M_SBC(M_RD_XIY()); }
static void sbc_a_a()   { M_SBC(R.AF.B.h); }
static void sbc_a_b()   { M_SBC(R.BC.B.h); }
static void sbc_a_c()   { M_SBC(R.BC.B.l); }
static void sbc_a_d()   { M_SBC(R.DE.B.h); }
static void sbc_a_e()   { M_SBC(R.DE.B.l); }
static void sbc_a_h()   { M_SBC(R.HL.B.h); }
static void sbc_a_l()   { M_SBC(R.HL.B.l); }
static void sbc_a_ixh() { M_SBC(R.IX.B.h); }
static void sbc_a_ixl() { M_SBC(R.IX.B.l); }
static void sbc_a_iyh() { M_SBC(R.IY.B.h); }
static void sbc_a_iyl() { M_SBC(R.IY.B.l); }

static void sbc_hl_bc() { M_SBCW(R.BC.W.l); }
static void sbc_hl_de() { M_SBCW(R.DE.W.l); }
static void sbc_hl_hl() { M_SBCW(R.HL.W.l); }
static void sbc_hl_sp() { M_SBCW(R.SP.W.l); }

static void scf() { R.AF.B.l = (R.AF.B.l&0xEC)|C_FLAG; }

static void set_0_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(0, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_0_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(0, i);
	M_WRMEM(j, i);
}
static void set_0_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(0, i);
	M_WRMEM(j, i);
}
static void set_0_a() { M_SET(0, R.AF.B.h); }
static void set_0_b() { M_SET(0, R.BC.B.h); }
static void set_0_c() { M_SET(0, R.BC.B.l); }
static void set_0_d() { M_SET(0, R.DE.B.h); }
static void set_0_e() { M_SET(0, R.DE.B.l); }
static void set_0_h() { M_SET(0, R.HL.B.h); }
static void set_0_l() { M_SET(0, R.HL.B.l); }

static void set_1_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(1, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_1_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(1, i);
	M_WRMEM(j, i);
}
static void set_1_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(1, i);
	M_WRMEM(j, i);
}
static void set_1_a() { M_SET(1, R.AF.B.h); }
static void set_1_b() { M_SET(1, R.BC.B.h); }
static void set_1_c() { M_SET(1, R.BC.B.l); }
static void set_1_d() { M_SET(1, R.DE.B.h); }
static void set_1_e() { M_SET(1, R.DE.B.l); }
static void set_1_h() { M_SET(1, R.HL.B.h); }
static void set_1_l() { M_SET(1, R.HL.B.l); }

static void set_2_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(2, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_2_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(2, i);
	M_WRMEM(j, i);
}
static void set_2_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(2, i);
	M_WRMEM(j, i);
}
static void set_2_a() { M_SET(2, R.AF.B.h); }
static void set_2_b() { M_SET(2, R.BC.B.h); }
static void set_2_c() { M_SET(2, R.BC.B.l); }
static void set_2_d() { M_SET(2, R.DE.B.h); }
static void set_2_e() { M_SET(2, R.DE.B.l); }
static void set_2_h() { M_SET(2, R.HL.B.h); }
static void set_2_l() { M_SET(2, R.HL.B.l); }

static void set_3_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(3, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_3_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(3, i);
	M_WRMEM(j, i);
}
static void set_3_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(3, i);
	M_WRMEM(j, i);
}
static void set_3_a() { M_SET(3, R.AF.B.h); }
static void set_3_b() { M_SET(3, R.BC.B.h); }
static void set_3_c() { M_SET(3, R.BC.B.l); }
static void set_3_d() { M_SET(3, R.DE.B.h); }
static void set_3_e() { M_SET(3, R.DE.B.l); }
static void set_3_h() { M_SET(3, R.HL.B.h); }
static void set_3_l() { M_SET(3, R.HL.B.l); }

static void set_4_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(4, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_4_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(4, i);
	M_WRMEM(j, i);
}
static void set_4_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(4, i);
	M_WRMEM(j, i);
}
static void set_4_a() { M_SET(4, R.AF.B.h); }
static void set_4_b() { M_SET(4, R.BC.B.h); }
static void set_4_c() { M_SET(4, R.BC.B.l); }
static void set_4_d() { M_SET(4, R.DE.B.h); }
static void set_4_e() { M_SET(4, R.DE.B.l); }
static void set_4_h() { M_SET(4, R.HL.B.h); }
static void set_4_l() { M_SET(4, R.HL.B.l); }

static void set_5_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(5, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_5_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(5, i);
	M_WRMEM(j, i);
}
static void set_5_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(5, i);
	M_WRMEM(j, i);
}
static void set_5_a() { M_SET(5, R.AF.B.h); }
static void set_5_b() { M_SET(5, R.BC.B.h); }
static void set_5_c() { M_SET(5, R.BC.B.l); }
static void set_5_d() { M_SET(5, R.DE.B.h); }
static void set_5_e() { M_SET(5, R.DE.B.l); }
static void set_5_h() { M_SET(5, R.HL.B.h); }
static void set_5_l() { M_SET(5, R.HL.B.l); }

static void set_6_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(6, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_6_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(6, i);
	M_WRMEM(j, i);
}
static void set_6_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(6, i);
	M_WRMEM(j, i);
}
static void set_6_a() { M_SET(6, R.AF.B.h); }
static void set_6_b() { M_SET(6, R.BC.B.h); }
static void set_6_c() { M_SET(6, R.BC.B.l); }
static void set_6_d() { M_SET(6, R.DE.B.h); }
static void set_6_e() { M_SET(6, R.DE.B.l); }
static void set_6_h() { M_SET(6, R.HL.B.h); }
static void set_6_l() { M_SET(6, R.HL.B.l); }

static void set_7_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SET(7, i);
	M_WRMEM(R.HL.W.l, i);
}
static void set_7_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SET(7, i);
	M_WRMEM(j, i);
}
static void set_7_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SET(7, i);
	M_WRMEM(j, i);
}
static void set_7_a() { M_SET(7, R.AF.B.h); }
static void set_7_b() { M_SET(7, R.BC.B.h); }
static void set_7_c() { M_SET(7, R.BC.B.l); }
static void set_7_d() { M_SET(7, R.DE.B.h); }
static void set_7_e() { M_SET(7, R.DE.B.l); }
static void set_7_h() { M_SET(7, R.HL.B.h); }
static void set_7_l() { M_SET(7, R.HL.B.l); }

static void sla_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SLA(i);
	M_WRMEM(R.HL.W.l, i);
}
static void sla_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SLA(i);
	M_WRMEM(j, i);
}
static void sla_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SLA(i);
	M_WRMEM(j, i);
}
static void sla_a() { M_SLA(R.AF.B.h); }
static void sla_b() { M_SLA(R.BC.B.h); }
static void sla_c() { M_SLA(R.BC.B.l); }
static void sla_d() { M_SLA(R.DE.B.h); }
static void sla_e() { M_SLA(R.DE.B.l); }
static void sla_h() { M_SLA(R.HL.B.h); }
static void sla_l() { M_SLA(R.HL.B.l); }

static void sll_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SLL(i);
	M_WRMEM(R.HL.W.l, i);
}
static void sll_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SLL(i);
	M_WRMEM(j, i);
}
static void sll_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SLL(i);
	M_WRMEM(j, i);
}
static void sll_a() { M_SLL(R.AF.B.h); }
static void sll_b() { M_SLL(R.BC.B.h); }
static void sll_c() { M_SLL(R.BC.B.l); }
static void sll_d() { M_SLL(R.DE.B.h); }
static void sll_e() { M_SLL(R.DE.B.l); }
static void sll_h() { M_SLL(R.HL.B.h); }
static void sll_l() { M_SLL(R.HL.B.l); }

static void sra_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SRA(i);
	M_WRMEM(R.HL.W.l, i);
}
static void sra_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SRA(i);
	M_WRMEM(j, i);
}
static void sra_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SRA(i);
	M_WRMEM(j, i);
}
static void sra_a() { M_SRA(R.AF.B.h); }
static void sra_b() { M_SRA(R.BC.B.h); }
static void sra_c() { M_SRA(R.BC.B.l); }
static void sra_d() { M_SRA(R.DE.B.h); }
static void sra_e() { M_SRA(R.DE.B.l); }
static void sra_h() { M_SRA(R.HL.B.h); }
static void sra_l() { M_SRA(R.HL.B.l); }

static void srl_xhl() {
	byte i = M_RDMEM(R.HL.W.l);
	M_SRL(i);
	M_WRMEM(R.HL.W.l, i);
}
static void srl_xix() {
	int j = M_XIX();
	byte i = M_RDMEM(j);
	M_SRL(i);
	M_WRMEM(j, i);
}
static void srl_xiy() {
	int j = M_XIY();
	byte i = M_RDMEM(j);
	M_SRL(i);
	M_WRMEM(j, i);
}
static void srl_a() { M_SRL(R.AF.B.h); }
static void srl_b() { M_SRL(R.BC.B.h); }
static void srl_c() { M_SRL(R.BC.B.l); }
static void srl_d() { M_SRL(R.DE.B.h); }
static void srl_e() { M_SRL(R.DE.B.l); }
static void srl_h() { M_SRL(R.HL.B.h); }
static void srl_l() { M_SRL(R.HL.B.l); }

static void sub_xhl() { M_SUB(M_RD_XHL()); }
static void sub_xix() { M_SUB(M_RD_XIX()); }
static void sub_xiy() { M_SUB(M_RD_XIY()); }
static void sub_a()   { R.AF.W.l = Z_FLAG|N_FLAG; }
static void sub_b()   { M_SUB(R.BC.B.h); }
static void sub_c()   { M_SUB(R.BC.B.l); }
static void sub_d()   { M_SUB(R.DE.B.h); }
static void sub_e()   { M_SUB(R.DE.B.l); }
static void sub_h()   { M_SUB(R.HL.B.h); }
static void sub_l()   { M_SUB(R.HL.B.l); }
static void sub_ixh() { M_SUB(R.IX.B.h); }
static void sub_ixl() { M_SUB(R.IX.B.l); }
static void sub_iyh() { M_SUB(R.IY.B.h); }
static void sub_iyl() { M_SUB(R.IY.B.l); }
static void sub_byte(){ M_SUB(M_RDMEM_OPCODE()); }

static void xor_xhl() { M_XOR(M_RD_XHL()); }
static void xor_xix() { M_XOR(M_RD_XIX()); }
static void xor_xiy() { M_XOR(M_RD_XIY()); }
static void xor_a()   { R.AF.W.l = Z_FLAG|V_FLAG;}
static void xor_b()   { M_XOR(R.BC.B.h); }
static void xor_c()   { M_XOR(R.BC.B.l); }
static void xor_d()   { M_XOR(R.DE.B.h); }
static void xor_e()   { M_XOR(R.DE.B.l); }
static void xor_h()   { M_XOR(R.HL.B.h); }
static void xor_l()   { M_XOR(R.HL.B.l); }
static void xor_ixh() { M_XOR(R.IX.B.h); }
static void xor_ixl() { M_XOR(R.IX.B.l); }
static void xor_iyh() { M_XOR(R.IY.B.h); }
static void xor_iyl() { M_XOR(R.IY.B.l); }
static void xor_byte(){ M_XOR(M_RDMEM_OPCODE()); }

static void no_op() { --R.PC.W.l; }

#define patch nop
//static void patch() { Z80_Patch(&R); }

static void dd_cb();
static void fd_cb();
static void cb();
static void dd();
static void ed();
static void fd();
static void ei();
static void no_op_xx() { ++R.PC.W.l; }


#include "opc__Z80.h"
static void dd_cb() {
	unsigned opcode = M_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
	R.ICount += cycles_xx_cb[opcode];
	(*(opcode_dd_cb[opcode]))();
	++R.PC.W.l;
}
static void fd_cb() {
	unsigned opcode = M_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
	R.ICount += cycles_xx_cb[opcode];
	(*(opcode_fd_cb[opcode]))();
	++R.PC.W.l;
}
static void cb() {
	++R.R;
	unsigned opcode = M_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_cb[opcode];
	(*(opcode_cb[opcode]))();
}
static void dd() {
	++R.R;
	unsigned opcode = M_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_xx[opcode];
	(*(opcode_dd[opcode]))();
}
static void ed() {
	++R.R;
	unsigned opcode = M_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_ed[opcode];
	(*(opcode_ed[opcode]))();
}
static void fd () {
	++R.R;
	unsigned opcode = M_RDOP(R.PC.W.l);
	R.PC.W.l++;
	R.ICount += cycles_xx[opcode];
	(*(opcode_fd[opcode]))();
}

static void ei() {
	// If interrupts were disabled, execute one more instruction and check the
	// IRQ line. If not, simply set interrupt flip-flop 2
	if (!R.IFF1) {
		R.IFF1 = R.IFF2 = 1;
		++R.R;
		unsigned opcode = M_RDOP(R.PC.W.l);
		R.PC.W.l++;
		R.ICount += cycles_main[opcode];
		(*(opcode_main[opcode]))(); // TODO Gives wrong timing for the moment !!
		if (MSXMotherBoard::instance()->IRQstatus())
			Interrupt(Z80_NORM_INT);
	} else
		R.IFF2 = 1;
}



/****************************************************************************/
/* Initialise the various lookup tables used by the emulation code          */
/****************************************************************************/
void InitTables ()
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
/* Issue an interrupt if necessary                                          */
/****************************************************************************/
void Interrupt (int j)
{
	printf("In interrupt routine:\n");
	printf("if (j==Z80_IGNORE_INT) return; %i\n", (j==Z80_IGNORE_INT));
	printf("if (j==Z80_NMI_INT || R.IFF1) %i || %i \n", (j==Z80_NMI_INT),R.IFF1);
	if (j==Z80_IGNORE_INT) return;
	if (j==Z80_NMI_INT || R.IFF1) {
		// Clear interrupt flip-flop 1
		printf("  R.IFF1 : %i ",R.IFF1 );
		R.IFF1=0;
		printf("  R.IFF1 : %i R.IM : %i \n",R.IFF1,R.IM );
		// Check if processor was halted
		if (R.HALT) {
			++R.PC.W.l;
			R.HALT=0;
		}
		if (j==Z80_NMI_INT) {
			M_PUSH (R.PC.W.l);
			R.PC.W.l=0x0066;
		} else {
			if (R.IM==2) {
				// Interrupt mode 2. Call [R.I:databyte]
				M_PUSH (R.PC.W.l);
				R.PC.W.l=M_RDMEM_WORD((j&255)|(R.I<<8));
			} else if (R.IM==1) {
				// Interrupt mode 1. RST 38h
					R.ICount+=cycles_main[0xFF];
					(*(opcode_main[0xFF]))();
			} else {
				// Interrupt mode 0. We check for CALL and JP instructions, if neither
				// of these were found we assume a 1 byte opcode was placed on the
				// databus
				switch (j&0xFF0000) {
				case 0xCD:
					M_PUSH(R.PC.W.l);
				case 0xC3:
					R.PC.W.l=j&0xFFFF;
					break;
				default:
					j&=255;
					R.ICount+=cycles_main[j];
					(*(opcode_main[j]))();
					break;
				}
			}
		}
	}
}

/****************************************************************************/
/* Execute IPeriod T-States.                                                */
/****************************************************************************/
void Z80_SingleInstruction() 
{
	#ifdef DEBUG
		word start_pc = R.PC.W.l;
	#endif
	++R.R;
	unsigned opcode = M_RDOP(R.PC.W.l++);
	R.ICount = cycles_main[opcode];	// instead of R.Icount=0
	(*(opcode_main[opcode]))();;	// R.ICount can be raised extra
	//  TODO: still need to adapt all other code to change the CurrentCPUTime 
	#ifdef DEBUG
		printf("%04x : instruction ", start_pc);
		Z80_Dasm(&debugmemory[start_pc], to_print_string, start_pc );
		printf("%s\n", to_print_string );
		printf("      A=%02x F=%02x \n", R.AF.B.h, R.AF.B.l);
		printf("      BC=%04x DE=%04x HL=%04x \n", R.BC.W.l, R.DE.W.l, R.HL.W.l);
	#endif
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80_SetWaitStates (int n)
{
	for (int i=0; i<256; ++i) {
		cycles_main[i] += n;
		cycles_cb[i]   += n;
		cycles_ed[i]   += n;
		cycles_xx[i]   += n;
	}
}


/****************************************************************************/
/* Reset the CPU emulation core                                             */
/* Set registers to their initial values                                    */
/* and make the Z80 the starting CPU                                        */
/****************************************************************************/
void Reset_CPU ()
{
	//Clear all data
	memset (&R,0,sizeof(CPU_Regs));
	R.SP.W.l = 0xF000;
	R.IX.W.l = 0xFFFF;
	R.IY.W.l = 0xFFFF;
	R.R = (byte)rand();
}

//TODO need cleanup

/****************************************************************************/
/* Input a byte from given I/O port                                         */
/****************************************************************************/
byte Z80_In (byte port) {
	return MSXMotherBoard::instance()->readIO(port, MSXCPU::instance()->getCurrentTime());
}


/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (byte port,byte value) {
	 MSXMotherBoard::instance()->writeIO(port, value, MSXCPU::instance()->getCurrentTime());
}
   
/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
byte Z80_RDMEM(word A) {
#ifdef DEBUG
	debugmemory[A]=MSXMotherBoard::instance()->readMem(A, MSXCPU::instance()->getCurrentTime());
	return debugmemory[A];
#else
	return  MSXMotherBoard::instance()->readMem(A, MSXCPU::instance()->getCurrentTime());
#endif
}

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
void Z80_WRMEM(word A,byte V) {
	 MSXMotherBoard::instance()->writeMem(A,V, MSXCPU::instance()->getCurrentTime());
	 // No debugmemory[A] here otherwise self-modifying code could 
	 // alter the executing code before the disassembled opcode
	 // is printed;
}


