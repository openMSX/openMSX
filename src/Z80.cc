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

//debugercode
#include "Z80Dasm.h"

#include "Z80.hh"
/*
extern "C++" 
{
#include "MSXZ80.h"
}
*/
// added JYD 22-01-2001
int Z80_IRQ;
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

void Interrupt(int j);
static void ei(void);

#define S_FLAG          0x80
#define Z_FLAG          0x40
#define H_FLAG          0x10
#define V_FLAG          0x04
#define N_FLAG          0x02
#define C_FLAG          0x01

#define M_SKIP_CALL     R.PC.W.l+=2
#define M_SKIP_JP       R.PC.W.l+=2
#define M_SKIP_JR       R.PC.W.l+=1
#define M_SKIP_RET

// defined in header file: static CPU_Regs R;
//CPU_Regs *R;
//static CPU_Regs R_Z80;
//static CPU_Regs R_R800;
int Z80_Running=1;
//int CPU_IPeriod=50000;
//int CPU_ICount=50000;

#ifdef DEBUG
int Z80_Trace=0;
int Z80_Trap=-1;
#endif
#ifdef TRACE
static unsigned pc_trace[256];
static unsigned pc_count=0;
#endif

static byte PTable[512];
static byte ZSTable[512];
static byte ZSPTable[512];
#include "Z80DAA.h"

typedef void (*opcode_fn) (void);

//static unsigned *cycles_main;
//static unsigned *cycles_cb;
//static unsigned *cycles_xx_cb;
//static unsigned *cycles_xx;
//static unsigned *cycles_ed;
//static opcode_fn *opcode_dd_cb;
//static opcode_fn *opcode_fd_cb;
//static opcode_fn *opcode_cb;
//static opcode_fn *opcode_dd;
//static opcode_fn *opcode_ed;
//static opcode_fn *opcode_fd;
//static opcode_fn *opcode_main;



#define M_C     (R.AF.B.l&C_FLAG)
#define M_NC    (!M_C)
#define M_Z     (R.AF.B.l&Z_FLAG)
#define M_NZ    (!M_Z)
#define M_M     (R.AF.B.l&S_FLAG)
#define M_P     (!M_M)
#define M_PE    (R.AF.B.l&V_FLAG)
#define M_PO    (!M_PE)

/* Get next opcode argument and increment program counter */
INLINE byte M_RDMEM_OPCODE (void)
{
 byte retval;
 retval=M_RDOP_ARG(R.PC.W.l);
 R.PC.W.l++;
 return retval;
}

// op 28,08,2001 bugje gevonden was byte 
//INLINE unsigned M_RDMEM_WORD (dword A)
INLINE word M_RDMEM_WORD (word A)
{
 int i;
 i=M_RDMEM(A);
 i+=M_RDMEM(((A)+1)&0xFFFF)<<8;
 return i;
}

//INLINE void M_WRMEM_WORD (dword A,word V)
INLINE void M_WRMEM_WORD (word A,word V)
{
 M_WRMEM (A,V&255);
 M_WRMEM (((A)+1)&0xFFFF,V>>8);
}

// op 28,08,2001 bugje gevonden was byte 
INLINE word M_RDMEM_OPCODE_WORD (void)
{
 int i;
 i=M_RDMEM_OPCODE();
 i+=M_RDMEM_OPCODE()<<8;
 return i;
}

#define M_XIX       ((R.IX.W.l+(offset)M_RDMEM_OPCODE())&0xFFFF)
#define M_XIY       ((R.IY.W.l+(offset)M_RDMEM_OPCODE())&0xFFFF)
#define M_RD_XHL    M_RDMEM(R.HL.W.l)
INLINE byte M_RD_XIX(void)
{
 int i;
 i=M_XIX;
 return M_RDMEM(i);
}
INLINE byte M_RD_XIY(void)
{
 int i;
 i=M_XIY;
 return M_RDMEM(i);
}
INLINE void M_WR_XIX(byte a)
{
 int i;
 i=M_XIX;
 M_WRMEM(i,a);
}
INLINE void M_WR_XIY(byte a)
{
 int i;
 i=M_XIY;
 M_WRMEM(i,a);
}

#ifdef X86_ASM
#include "Z80CDx86.h"
#else
#include "Z80Codes.h"
#endif

static void adc_a_xhl(void) { byte i=M_RD_XHL; M_ADC(i); }
static void adc_a_xix(void) { byte i=M_RD_XIX(); M_ADC(i); }
static void adc_a_xiy(void) { byte i=M_RD_XIY(); M_ADC(i); }
static void adc_a_a(void) { M_ADC(R.AF.B.h); }
static void adc_a_b(void) { M_ADC(R.BC.B.h); }
static void adc_a_c(void) { M_ADC(R.BC.B.l); }
static void adc_a_d(void) { M_ADC(R.DE.B.h); }
static void adc_a_e(void) { M_ADC(R.DE.B.l); }
static void adc_a_h(void) { M_ADC(R.HL.B.h); }
static void adc_a_l(void) { M_ADC(R.HL.B.l); }
static void adc_a_ixl(void) { M_ADC(R.IX.B.l); }
static void adc_a_ixh(void) { M_ADC(R.IX.B.h); }
static void adc_a_iyl(void) { M_ADC(R.IY.B.l); }
static void adc_a_iyh(void) { M_ADC(R.IY.B.h); }
static void adc_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_ADC(i); }

static void adc_hl_bc(void) { M_ADCW(BC); }
static void adc_hl_de(void) { M_ADCW(DE); }
static void adc_hl_hl(void) { M_ADCW(HL); }
static void adc_hl_sp(void) { M_ADCW(SP); }

static void add_a_xhl(void) { byte i=M_RD_XHL; M_ADD(i); }
static void add_a_xix(void) { byte i=M_RD_XIX(); M_ADD(i); }
static void add_a_xiy(void) { byte i=M_RD_XIY(); M_ADD(i); }
static void add_a_a(void) { M_ADD(R.AF.B.h); }
static void add_a_b(void) { M_ADD(R.BC.B.h); }
static void add_a_c(void) { M_ADD(R.BC.B.l); }
static void add_a_d(void) { M_ADD(R.DE.B.h); }
static void add_a_e(void) { M_ADD(R.DE.B.l); }
static void add_a_h(void) { M_ADD(R.HL.B.h); }
static void add_a_l(void) { M_ADD(R.HL.B.l); }
static void add_a_ixl(void) { M_ADD(R.IX.B.l); }
static void add_a_ixh(void) { M_ADD(R.IX.B.h); }
static void add_a_iyl(void) { M_ADD(R.IY.B.l); }
static void add_a_iyh(void) { M_ADD(R.IY.B.h); }
static void add_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_ADD(i); }

static void add_hl_bc(void) { M_ADDW(HL,BC); }
static void add_hl_de(void) { M_ADDW(HL,DE); }
static void add_hl_hl(void) { M_ADDW(HL,HL); }
static void add_hl_sp(void) { M_ADDW(HL,SP); }
static void add_ix_bc(void) { M_ADDW(IX,BC); }
static void add_ix_de(void) { M_ADDW(IX,DE); }
static void add_ix_ix(void) { M_ADDW(IX,IX); }
static void add_ix_sp(void) { M_ADDW(IX,SP); }
static void add_iy_bc(void) { M_ADDW(IY,BC); }
static void add_iy_de(void) { M_ADDW(IY,DE); }
static void add_iy_iy(void) { M_ADDW(IY,IY); }
static void add_iy_sp(void) { M_ADDW(IY,SP); }

static void and_xhl(void) { byte i=M_RD_XHL; M_AND(i); }
static void and_xix(void) { byte i=M_RD_XIX(); M_AND(i); }
static void and_xiy(void) { byte i=M_RD_XIY(); M_AND(i); }
static void and_a(void) { R.AF.B.l=ZSPTable[R.AF.B.h]|H_FLAG; }
static void and_b(void) { M_AND(R.BC.B.h); }
static void and_c(void) { M_AND(R.BC.B.l); }
static void and_d(void) { M_AND(R.DE.B.h); }
static void and_e(void) { M_AND(R.DE.B.l); }
static void and_h(void) { M_AND(R.HL.B.h); }
static void and_l(void) { M_AND(R.HL.B.l); }
static void and_ixh(void) { M_AND(R.IX.B.h); }
static void and_ixl(void) { M_AND(R.IX.B.l); }
static void and_iyh(void) { M_AND(R.IY.B.h); }
static void and_iyl(void) { M_AND(R.IY.B.l); }
static void and_byte(void) { byte i=M_RDMEM_OPCODE(); M_AND(i); }

static void bit_0_xhl(void) { byte i=M_RD_XHL; M_BIT(0,i); }
static void bit_0_xix(void) { byte i=M_RD_XIX(); M_BIT(0,i); }
static void bit_0_xiy(void) { byte i=M_RD_XIY(); M_BIT(0,i); }
static void bit_0_a(void) { M_BIT(0,R.AF.B.h); }
static void bit_0_b(void) { M_BIT(0,R.BC.B.h); }
static void bit_0_c(void) { M_BIT(0,R.BC.B.l); }
static void bit_0_d(void) { M_BIT(0,R.DE.B.h); }
static void bit_0_e(void) { M_BIT(0,R.DE.B.l); }
static void bit_0_h(void) { M_BIT(0,R.HL.B.h); }
static void bit_0_l(void) { M_BIT(0,R.HL.B.l); }

static void bit_1_xhl(void) { byte i=M_RD_XHL; M_BIT(1,i); }
static void bit_1_xix(void) { byte i=M_RD_XIX(); M_BIT(1,i); }
static void bit_1_xiy(void) { byte i=M_RD_XIY(); M_BIT(1,i); }
static void bit_1_a(void) { M_BIT(1,R.AF.B.h); }
static void bit_1_b(void) { M_BIT(1,R.BC.B.h); }
static void bit_1_c(void) { M_BIT(1,R.BC.B.l); }
static void bit_1_d(void) { M_BIT(1,R.DE.B.h); }
static void bit_1_e(void) { M_BIT(1,R.DE.B.l); }
static void bit_1_h(void) { M_BIT(1,R.HL.B.h); }
static void bit_1_l(void) { M_BIT(1,R.HL.B.l); }

static void bit_2_xhl(void) { byte i=M_RD_XHL; M_BIT(2,i); }
static void bit_2_xix(void) { byte i=M_RD_XIX(); M_BIT(2,i); }
static void bit_2_xiy(void) { byte i=M_RD_XIY(); M_BIT(2,i); }
static void bit_2_a(void) { M_BIT(2,R.AF.B.h); }
static void bit_2_b(void) { M_BIT(2,R.BC.B.h); }
static void bit_2_c(void) { M_BIT(2,R.BC.B.l); }
static void bit_2_d(void) { M_BIT(2,R.DE.B.h); }
static void bit_2_e(void) { M_BIT(2,R.DE.B.l); }
static void bit_2_h(void) { M_BIT(2,R.HL.B.h); }
static void bit_2_l(void) { M_BIT(2,R.HL.B.l); }

static void bit_3_xhl(void) { byte i=M_RD_XHL; M_BIT(3,i); }
static void bit_3_xix(void) { byte i=M_RD_XIX(); M_BIT(3,i); }
static void bit_3_xiy(void) { byte i=M_RD_XIY(); M_BIT(3,i); }
static void bit_3_a(void) { M_BIT(3,R.AF.B.h); }
static void bit_3_b(void) { M_BIT(3,R.BC.B.h); }
static void bit_3_c(void) { M_BIT(3,R.BC.B.l); }
static void bit_3_d(void) { M_BIT(3,R.DE.B.h); }
static void bit_3_e(void) { M_BIT(3,R.DE.B.l); }
static void bit_3_h(void) { M_BIT(3,R.HL.B.h); }
static void bit_3_l(void) { M_BIT(3,R.HL.B.l); }

static void bit_4_xhl(void) { byte i=M_RD_XHL; M_BIT(4,i); }
static void bit_4_xix(void) { byte i=M_RD_XIX(); M_BIT(4,i); }
static void bit_4_xiy(void) { byte i=M_RD_XIY(); M_BIT(4,i); }
static void bit_4_a(void) { M_BIT(4,R.AF.B.h); }
static void bit_4_b(void) { M_BIT(4,R.BC.B.h); }
static void bit_4_c(void) { M_BIT(4,R.BC.B.l); }
static void bit_4_d(void) { M_BIT(4,R.DE.B.h); }
static void bit_4_e(void) { M_BIT(4,R.DE.B.l); }
static void bit_4_h(void) { M_BIT(4,R.HL.B.h); }
static void bit_4_l(void) { M_BIT(4,R.HL.B.l); }

static void bit_5_xhl(void) { byte i=M_RD_XHL; M_BIT(5,i); }
static void bit_5_xix(void) { byte i=M_RD_XIX(); M_BIT(5,i); }
static void bit_5_xiy(void) { byte i=M_RD_XIY(); M_BIT(5,i); }
static void bit_5_a(void) { M_BIT(5,R.AF.B.h); }
static void bit_5_b(void) { M_BIT(5,R.BC.B.h); }
static void bit_5_c(void) { M_BIT(5,R.BC.B.l); }
static void bit_5_d(void) { M_BIT(5,R.DE.B.h); }
static void bit_5_e(void) { M_BIT(5,R.DE.B.l); }
static void bit_5_h(void) { M_BIT(5,R.HL.B.h); }
static void bit_5_l(void) { M_BIT(5,R.HL.B.l); }

static void bit_6_xhl(void) { byte i=M_RD_XHL; M_BIT(6,i); }
static void bit_6_xix(void) { byte i=M_RD_XIX(); M_BIT(6,i); }
static void bit_6_xiy(void) { byte i=M_RD_XIY(); M_BIT(6,i); }
static void bit_6_a(void) { M_BIT(6,R.AF.B.h); }
static void bit_6_b(void) { M_BIT(6,R.BC.B.h); }
static void bit_6_c(void) { M_BIT(6,R.BC.B.l); }
static void bit_6_d(void) { M_BIT(6,R.DE.B.h); }
static void bit_6_e(void) { M_BIT(6,R.DE.B.l); }
static void bit_6_h(void) { M_BIT(6,R.HL.B.h); }
static void bit_6_l(void) { M_BIT(6,R.HL.B.l); }

static void bit_7_xhl(void) { byte i=M_RD_XHL; M_BIT(7,i); }
static void bit_7_xix(void) { byte i=M_RD_XIX(); M_BIT(7,i); }
static void bit_7_xiy(void) { byte i=M_RD_XIY(); M_BIT(7,i); }
static void bit_7_a(void) { M_BIT(7,R.AF.B.h); }
static void bit_7_b(void) { M_BIT(7,R.BC.B.h); }
static void bit_7_c(void) { M_BIT(7,R.BC.B.l); }
static void bit_7_d(void) { M_BIT(7,R.DE.B.h); }
static void bit_7_e(void) { M_BIT(7,R.DE.B.l); }
static void bit_7_h(void) { M_BIT(7,R.HL.B.h); }
static void bit_7_l(void) { M_BIT(7,R.HL.B.l); }

static void call_c(void) { if (M_C) { M_CALL; } else { M_SKIP_CALL; } }
static void call_m(void) { if (M_M) { M_CALL; } else { M_SKIP_CALL; } }
static void call_nc(void) { if (M_NC) { M_CALL; } else { M_SKIP_CALL; } }
static void call_nz(void) { if (M_NZ) { M_CALL; } else { M_SKIP_CALL; } }
static void call_p(void) { if (M_P) { M_CALL; } else { M_SKIP_CALL; } }
static void call_pe(void) { if (M_PE) { M_CALL; } else { M_SKIP_CALL; } }
static void call_po(void) { if (M_PO) { M_CALL; } else { M_SKIP_CALL; } }
static void call_z(void) { if (M_Z) { M_CALL; } else { M_SKIP_CALL; } }
static void call(void) { M_CALL; }

static void ccf(void) { R.AF.B.l=((R.AF.B.l&0xED)|((R.AF.B.l&1)<<4))^1; }

static void cp_xhl(void) { byte i=M_RD_XHL; M_CP(i); }
static void cp_xix(void) { byte i=M_RD_XIX(); M_CP(i); }
static void cp_xiy(void) { byte i=M_RD_XIY(); M_CP(i); }
static void cp_a(void) { M_CP(R.AF.B.h); }
static void cp_b(void) { M_CP(R.BC.B.h); }
static void cp_c(void) { M_CP(R.BC.B.l); }
static void cp_d(void) { M_CP(R.DE.B.h); }
static void cp_e(void) { M_CP(R.DE.B.l); }
static void cp_h(void) { M_CP(R.HL.B.h); }
static void cp_l(void) { M_CP(R.HL.B.l); }
static void cp_ixh(void) { M_CP(R.IX.B.h); }
static void cp_ixl(void) { M_CP(R.IX.B.l); }
static void cp_iyh(void) { M_CP(R.IY.B.h); }
static void cp_iyl(void) { M_CP(R.IY.B.l); }
static void cp_byte(void) { byte i=M_RDMEM_OPCODE(); M_CP(i); }

static void cpd(void)
{
 byte i,j;
 i=M_RDMEM(R.HL.W.l);
 j=R.AF.B.h-i;
 --R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[j]|
          ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}

static void cpdr(void)
{
 cpd ();
 if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

static void cpi(void)
{
 byte i,j;
 i=M_RDMEM(R.HL.W.l);
 j=R.AF.B.h-i;
 ++R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[j]|
          ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.W.l? V_FLAG:0)|N_FLAG;
}

static void cpir(void)
{
 cpi ();
 if (R.BC.W.l && !(R.AF.B.l&Z_FLAG)) { R.ICount+=5; R.PC.W.l-=2; }
}

static void cpl(void) { R.AF.B.h^=0xFF; R.AF.B.l|=(H_FLAG|N_FLAG); }

static void daa(void)
{
 int i;
 i=R.AF.B.h;
 if (R.AF.B.l&C_FLAG) i|=256;
 if (R.AF.B.l&H_FLAG) i|=512;
 if (R.AF.B.l&N_FLAG) i|=1024;
 R.AF.W.l=DAATable[i];
};

static void dec_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_DEC(i);
 M_WRMEM(R.HL.W.l,i);
}
static void dec_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_DEC(i);
 M_WRMEM(j,i);
}
static void dec_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_DEC(i);
 M_WRMEM(j,i);
}
static void dec_a(void) { M_DEC(R.AF.B.h); }
static void dec_b(void) { M_DEC(R.BC.B.h); }
static void dec_c(void) { M_DEC(R.BC.B.l); }
static void dec_d(void) { M_DEC(R.DE.B.h); }
static void dec_e(void) { M_DEC(R.DE.B.l); }
static void dec_h(void) { M_DEC(R.HL.B.h); }
static void dec_l(void) { M_DEC(R.HL.B.l); }
static void dec_ixh(void) { M_DEC(R.IX.B.h); }
static void dec_ixl(void) { M_DEC(R.IX.B.l); }
static void dec_iyh(void) { M_DEC(R.IY.B.h); }
static void dec_iyl(void) { M_DEC(R.IY.B.l); }

static void dec_bc(void) { --R.BC.W.l; }
static void dec_de(void) { --R.DE.W.l; }
static void dec_hl(void) { --R.HL.W.l; }
static void dec_ix(void) { --R.IX.W.l; }
static void dec_iy(void) { --R.IY.W.l; }
static void dec_sp(void) { --R.SP.W.l; }

static void di(void) { R.IFF1=R.IFF2=0; }

static void djnz(void) { if (--R.BC.B.h) { M_JR; } else { M_SKIP_JR; } }

static void ex_xsp_hl(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.W.l);
 M_WRMEM_WORD(R.SP.W.l,R.HL.W.l);
 R.HL.W.l=i;
}

static void ex_xsp_ix(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.W.l);
 M_WRMEM_WORD(R.SP.W.l,R.IX.W.l);
 R.IX.W.l=i;
}

static void ex_xsp_iy(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.W.l);
 M_WRMEM_WORD(R.SP.W.l,R.IY.W.l);
 R.IY.W.l=i;
}

static void ex_af_af(void)
{
 int i;
 i=R.AF.W.l;
 R.AF.W.l=R.AF2.W.l;
 R.AF2.W.l=i;
}

static void ex_de_hl(void)
{
 int i;
 i=R.DE.W.l;
 R.DE.W.l=R.HL.W.l;
 R.HL.W.l=i;
}

static void exx(void)
{
 int i;
 i=R.BC.W.l;
 R.BC.W.l=R.BC2.W.l;
 R.BC2.W.l=i;
 i=R.DE.W.l;
 R.DE.W.l=R.DE2.W.l;
 R.DE2.W.l=i;
 i=R.HL.W.l;
 R.HL.W.l=R.HL2.W.l;
 R.HL2.W.l=i;
}

static void halt(void)
{
 --R.PC.W.l;
 R.HALT=1;
 if (R.ICount>0) R.ICount=0;
 //TODO: Set CurrentCPUTime to targetCPUTime
}

static void im_0(void) { R.IM=0; }
static void im_1(void) { R.IM=1; }
static void im_2(void) { R.IM=2; }

static void in_a_c(void) { M_IN(R.AF.B.h); }
static void in_b_c(void) { M_IN(R.BC.B.h); }
static void in_c_c(void) { M_IN(R.BC.B.l); }
static void in_d_c(void) { M_IN(R.DE.B.h); }
static void in_e_c(void) { M_IN(R.DE.B.l); }
static void in_h_c(void) { M_IN(R.HL.B.h); }
static void in_l_c(void) { M_IN(R.HL.B.l); }
static void in_0_c(void) { byte i; M_IN(i); }

static void in_a_byte(void)
{
 byte i=M_RDMEM_OPCODE();
 R.AF.B.h=DoIn(i,R.AF.B.h);
}

static void inc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_INC(i);
 M_WRMEM(R.HL.W.l,i);
}
static void inc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_INC(i);
 M_WRMEM(j,i);
}
static void inc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_INC(i);
 M_WRMEM(j,i);
}
static void inc_a(void) { M_INC(R.AF.B.h); }
static void inc_b(void) { M_INC(R.BC.B.h); }
static void inc_c(void) { M_INC(R.BC.B.l); }
static void inc_d(void) { M_INC(R.DE.B.h); }
static void inc_e(void) { M_INC(R.DE.B.l); }
static void inc_h(void) { M_INC(R.HL.B.h); }
static void inc_l(void) { M_INC(R.HL.B.l); }
static void inc_ixh(void) { M_INC(R.IX.B.h); }
static void inc_ixl(void) { M_INC(R.IX.B.l); }
static void inc_iyh(void) { M_INC(R.IY.B.h); }
static void inc_iyl(void) { M_INC(R.IY.B.l); }

static void inc_bc(void) { ++R.BC.W.l; }
static void inc_de(void) { ++R.DE.W.l; }
static void inc_hl(void) { ++R.HL.W.l; }
static void inc_ix(void) { ++R.IX.W.l; }
static void inc_iy(void) { ++R.IY.W.l; }
static void inc_sp(void) { ++R.SP.W.l; }

static void ind(void)
{
 --R.BC.B.h;
 M_WRMEM(R.HL.W.l,DoIn(R.BC.B.l,R.BC.B.h));
 --R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(N_FLAG|Z_FLAG);
}

static void indr(void)
{
 ind ();
 if (R.BC.B.h) { R.ICount+=5; R.PC.W.l-=2; }
}

static void ini(void)
{
 --R.BC.B.h;
 M_WRMEM(R.HL.W.l,DoIn(R.BC.B.l,R.BC.B.h));
 ++R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(N_FLAG|Z_FLAG);
}

static void inir(void)
{
 ini ();
 if (R.BC.B.h) { R.ICount+=5; R.PC.W.l-=2; }
}

static void jp(void) { M_JP; }
static void jp_hl(void) { R.PC.W.l=R.HL.W.l; }
static void jp_ix(void) { R.PC.W.l=R.IX.W.l; }
static void jp_iy(void) { R.PC.W.l=R.IY.W.l; }
static void jp_c(void) { if (M_C) { M_JP; } else { M_SKIP_JP; } }
static void jp_m(void) { if (M_M) { M_JP; } else { M_SKIP_JP; } }
static void jp_nc(void) { if (M_NC) { M_JP; } else { M_SKIP_JP; } }
static void jp_nz(void) { if (M_NZ) { M_JP; } else { M_SKIP_JP; } }
static void jp_p(void) { if (M_P) { M_JP; } else { M_SKIP_JP; } }
static void jp_pe(void) { if (M_PE) { M_JP; } else { M_SKIP_JP; } }
static void jp_po(void) { if (M_PO) { M_JP; } else { M_SKIP_JP; } }
static void jp_z(void) { if (M_Z) { M_JP; } else { M_SKIP_JP; } }

static void jr(void) { M_JR; }
static void jr_c(void) { if (M_C) { M_JR; } else { M_SKIP_JR; } }
static void jr_nc(void) { if (M_NC) { M_JR; } else { M_SKIP_JR; } }
static void jr_nz(void) { if (M_NZ) { M_JR; } else { M_SKIP_JR; } }
static void jr_z(void) { if (M_Z) { M_JR; } else { M_SKIP_JR; } }

static void ld_xbc_a(void) { M_WRMEM(R.BC.W.l,R.AF.B.h); }
static void ld_xde_a(void) { M_WRMEM(R.DE.W.l,R.AF.B.h); }
static void ld_xhl_a(void) { M_WRMEM(R.HL.W.l,R.AF.B.h); }
static void ld_xhl_b(void) { M_WRMEM(R.HL.W.l,R.BC.B.h); }
static void ld_xhl_c(void) { M_WRMEM(R.HL.W.l,R.BC.B.l); }
static void ld_xhl_d(void) { M_WRMEM(R.HL.W.l,R.DE.B.h); }
static void ld_xhl_e(void) { M_WRMEM(R.HL.W.l,R.DE.B.l); }
static void ld_xhl_h(void) { M_WRMEM(R.HL.W.l,R.HL.B.h); }
static void ld_xhl_l(void) { M_WRMEM(R.HL.W.l,R.HL.B.l); }
static void ld_xhl_byte(void) { byte i=M_RDMEM_OPCODE(); M_WRMEM(R.HL.W.l,i); }
static void ld_xix_a(void) { M_WR_XIX(R.AF.B.h); }
static void ld_xix_b(void) { M_WR_XIX(R.BC.B.h); }
static void ld_xix_c(void) { M_WR_XIX(R.BC.B.l); }
static void ld_xix_d(void) { M_WR_XIX(R.DE.B.h); }
static void ld_xix_e(void) { M_WR_XIX(R.DE.B.l); }
static void ld_xix_h(void) { M_WR_XIX(R.HL.B.h); }
static void ld_xix_l(void) { M_WR_XIX(R.HL.B.l); }
static void ld_xix_byte(void)
{
 int i,j;
 i=M_XIX;
 j=M_RDMEM_OPCODE();
 M_WRMEM(i,j);
}
static void ld_xiy_a(void) { M_WR_XIY(R.AF.B.h); }
static void ld_xiy_b(void) { M_WR_XIY(R.BC.B.h); }
static void ld_xiy_c(void) { M_WR_XIY(R.BC.B.l); }
static void ld_xiy_d(void) { M_WR_XIY(R.DE.B.h); }
static void ld_xiy_e(void) { M_WR_XIY(R.DE.B.l); }
static void ld_xiy_h(void) { M_WR_XIY(R.HL.B.h); }
static void ld_xiy_l(void) { M_WR_XIY(R.HL.B.l); }
static void ld_xiy_byte(void)
{
 int i,j;
 i=M_XIY;
 j=M_RDMEM_OPCODE();
 M_WRMEM(i,j);
}
static void ld_xbyte_a(void)
{ int i=M_RDMEM_OPCODE_WORD(); M_WRMEM(i,R.AF.B.h); }
static void ld_xword_bc(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.BC.W.l); }
static void ld_xword_de(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.DE.W.l); }
static void ld_xword_hl(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.HL.W.l); }
static void ld_xword_ix(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.IX.W.l); }
static void ld_xword_iy(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.IY.W.l); }
static void ld_xword_sp(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.SP.W.l); }
static void ld_a_xbc(void) { R.AF.B.h=M_RDMEM(R.BC.W.l); }
static void ld_a_xde(void) { R.AF.B.h=M_RDMEM(R.DE.W.l); }
static void ld_a_xhl(void) { R.AF.B.h=M_RD_XHL; }
static void ld_a_xix(void) { R.AF.B.h=M_RD_XIX(); }
static void ld_a_xiy(void) { R.AF.B.h=M_RD_XIY(); }
static void ld_a_xbyte(void)
{ int i=M_RDMEM_OPCODE_WORD(); R.AF.B.h=M_RDMEM(i); }

static void ld_a_byte(void) { R.AF.B.h=M_RDMEM_OPCODE(); }
static void ld_b_byte(void) { R.BC.B.h=M_RDMEM_OPCODE(); }
static void ld_c_byte(void) { R.BC.B.l=M_RDMEM_OPCODE(); }
static void ld_d_byte(void) { R.DE.B.h=M_RDMEM_OPCODE(); }
static void ld_e_byte(void) { R.DE.B.l=M_RDMEM_OPCODE(); }
static void ld_h_byte(void) { R.HL.B.h=M_RDMEM_OPCODE(); }
static void ld_l_byte(void) { R.HL.B.l=M_RDMEM_OPCODE(); }
static void ld_ixh_byte(void) { R.IX.B.h=M_RDMEM_OPCODE(); }
static void ld_ixl_byte(void) { R.IX.B.l=M_RDMEM_OPCODE(); }
static void ld_iyh_byte(void) { R.IY.B.h=M_RDMEM_OPCODE(); }
static void ld_iyl_byte(void) { R.IY.B.l=M_RDMEM_OPCODE(); }

static void ld_b_xhl(void) { R.BC.B.h=M_RD_XHL; }
static void ld_c_xhl(void) { R.BC.B.l=M_RD_XHL; }
static void ld_d_xhl(void) { R.DE.B.h=M_RD_XHL; }
static void ld_e_xhl(void) { R.DE.B.l=M_RD_XHL; }
static void ld_h_xhl(void) { R.HL.B.h=M_RD_XHL; }
static void ld_l_xhl(void) { R.HL.B.l=M_RD_XHL; }
static void ld_b_xix(void) { R.BC.B.h=M_RD_XIX(); }
static void ld_c_xix(void) { R.BC.B.l=M_RD_XIX(); }
static void ld_d_xix(void) { R.DE.B.h=M_RD_XIX(); }
static void ld_e_xix(void) { R.DE.B.l=M_RD_XIX(); }
static void ld_h_xix(void) { R.HL.B.h=M_RD_XIX(); }
static void ld_l_xix(void) { R.HL.B.l=M_RD_XIX(); }
static void ld_b_xiy(void) { R.BC.B.h=M_RD_XIY(); }
static void ld_c_xiy(void) { R.BC.B.l=M_RD_XIY(); }
static void ld_d_xiy(void) { R.DE.B.h=M_RD_XIY(); }
static void ld_e_xiy(void) { R.DE.B.l=M_RD_XIY(); }
static void ld_h_xiy(void) { R.HL.B.h=M_RD_XIY(); }
static void ld_l_xiy(void) { R.HL.B.l=M_RD_XIY(); }
static void ld_a_a(void) { }
static void ld_a_b(void) { R.AF.B.h=R.BC.B.h; }
static void ld_a_c(void) { R.AF.B.h=R.BC.B.l; }
static void ld_a_d(void) { R.AF.B.h=R.DE.B.h; }
static void ld_a_e(void) { R.AF.B.h=R.DE.B.l; }
static void ld_a_h(void) { R.AF.B.h=R.HL.B.h; }
static void ld_a_l(void) { R.AF.B.h=R.HL.B.l; }
static void ld_a_ixh(void) { R.AF.B.h=R.IX.B.h; }
static void ld_a_ixl(void) { R.AF.B.h=R.IX.B.l; }
static void ld_a_iyh(void) { R.AF.B.h=R.IY.B.h; }
static void ld_a_iyl(void) { R.AF.B.h=R.IY.B.l; }
static void ld_b_b(void) { }
static void ld_b_a(void) { R.BC.B.h=R.AF.B.h; }
static void ld_b_c(void) { R.BC.B.h=R.BC.B.l; }
static void ld_b_d(void) { R.BC.B.h=R.DE.B.h; }
static void ld_b_e(void) { R.BC.B.h=R.DE.B.l; }
static void ld_b_h(void) { R.BC.B.h=R.HL.B.h; }
static void ld_b_l(void) { R.BC.B.h=R.HL.B.l; }
static void ld_b_ixh(void) { R.BC.B.h=R.IX.B.h; }
static void ld_b_ixl(void) { R.BC.B.h=R.IX.B.l; }
static void ld_b_iyh(void) { R.BC.B.h=R.IY.B.h; }
static void ld_b_iyl(void) { R.BC.B.h=R.IY.B.l; }
static void ld_c_c(void) { }
static void ld_c_a(void) { R.BC.B.l=R.AF.B.h; }
static void ld_c_b(void) { R.BC.B.l=R.BC.B.h; }
static void ld_c_d(void) { R.BC.B.l=R.DE.B.h; }
static void ld_c_e(void) { R.BC.B.l=R.DE.B.l; }
static void ld_c_h(void) { R.BC.B.l=R.HL.B.h; }
static void ld_c_l(void) { R.BC.B.l=R.HL.B.l; }
static void ld_c_ixh(void) { R.BC.B.l=R.IX.B.h; }
static void ld_c_ixl(void) { R.BC.B.l=R.IX.B.l; }
static void ld_c_iyh(void) { R.BC.B.l=R.IY.B.h; }
static void ld_c_iyl(void) { R.BC.B.l=R.IY.B.l; }
static void ld_d_d(void) { }
static void ld_d_a(void) { R.DE.B.h=R.AF.B.h; }
static void ld_d_c(void) { R.DE.B.h=R.BC.B.l; }
static void ld_d_b(void) { R.DE.B.h=R.BC.B.h; }
static void ld_d_e(void) { R.DE.B.h=R.DE.B.l; }
static void ld_d_h(void) { R.DE.B.h=R.HL.B.h; }
static void ld_d_l(void) { R.DE.B.h=R.HL.B.l; }
static void ld_d_ixh(void) { R.DE.B.h=R.IX.B.h; }
static void ld_d_ixl(void) { R.DE.B.h=R.IX.B.l; }
static void ld_d_iyh(void) { R.DE.B.h=R.IY.B.h; }
static void ld_d_iyl(void) { R.DE.B.h=R.IY.B.l; }
static void ld_e_e(void) { }
static void ld_e_a(void) { R.DE.B.l=R.AF.B.h; }
static void ld_e_c(void) { R.DE.B.l=R.BC.B.l; }
static void ld_e_b(void) { R.DE.B.l=R.BC.B.h; }
static void ld_e_d(void) { R.DE.B.l=R.DE.B.h; }
static void ld_e_h(void) { R.DE.B.l=R.HL.B.h; }
static void ld_e_l(void) { R.DE.B.l=R.HL.B.l; }
static void ld_e_ixh(void) { R.DE.B.l=R.IX.B.h; }
static void ld_e_ixl(void) { R.DE.B.l=R.IX.B.l; }
static void ld_e_iyh(void) { R.DE.B.l=R.IY.B.h; }
static void ld_e_iyl(void) { R.DE.B.l=R.IY.B.l; }
static void ld_h_h(void) { }
static void ld_h_a(void) { R.HL.B.h=R.AF.B.h; }
static void ld_h_c(void) { R.HL.B.h=R.BC.B.l; }
static void ld_h_b(void) { R.HL.B.h=R.BC.B.h; }
static void ld_h_e(void) { R.HL.B.h=R.DE.B.l; }
static void ld_h_d(void) { R.HL.B.h=R.DE.B.h; }
static void ld_h_l(void) { R.HL.B.h=R.HL.B.l; }
static void ld_l_l(void) { }
static void ld_l_a(void) { R.HL.B.l=R.AF.B.h; }
static void ld_l_c(void) { R.HL.B.l=R.BC.B.l; }
static void ld_l_b(void) { R.HL.B.l=R.BC.B.h; }
static void ld_l_e(void) { R.HL.B.l=R.DE.B.l; }
static void ld_l_d(void) { R.HL.B.l=R.DE.B.h; }
static void ld_l_h(void) { R.HL.B.l=R.HL.B.h; }
static void ld_ixh_a(void) { R.IX.B.h=R.AF.B.h; }
static void ld_ixh_b(void) { R.IX.B.h=R.BC.B.h; }
static void ld_ixh_c(void) { R.IX.B.h=R.BC.B.l; }
static void ld_ixh_d(void) { R.IX.B.h=R.DE.B.h; }
static void ld_ixh_e(void) { R.IX.B.h=R.DE.B.l; }
static void ld_ixh_ixh(void) { }
static void ld_ixh_ixl(void) { R.IX.B.h=R.IX.B.l; }
static void ld_ixl_a(void) { R.IX.B.l=R.AF.B.h; }
static void ld_ixl_b(void) { R.IX.B.l=R.BC.B.h; }
static void ld_ixl_c(void) { R.IX.B.l=R.BC.B.l; }
static void ld_ixl_d(void) { R.IX.B.l=R.DE.B.h; }
static void ld_ixl_e(void) { R.IX.B.l=R.DE.B.l; }
static void ld_ixl_ixh(void) { R.IX.B.l=R.IX.B.h; }
static void ld_ixl_ixl(void) { }
static void ld_iyh_a(void) { R.IY.B.h=R.AF.B.h; }
static void ld_iyh_b(void) { R.IY.B.h=R.BC.B.h; }
static void ld_iyh_c(void) { R.IY.B.h=R.BC.B.l; }
static void ld_iyh_d(void) { R.IY.B.h=R.DE.B.h; }
static void ld_iyh_e(void) { R.IY.B.h=R.DE.B.l; }
static void ld_iyh_iyh(void) { }
static void ld_iyh_iyl(void) { R.IY.B.h=R.IY.B.l; }
static void ld_iyl_a(void) { R.IY.B.l=R.AF.B.h; }
static void ld_iyl_b(void) { R.IY.B.l=R.BC.B.h; }
static void ld_iyl_c(void) { R.IY.B.l=R.BC.B.l; }
static void ld_iyl_d(void) { R.IY.B.l=R.DE.B.h; }
static void ld_iyl_e(void) { R.IY.B.l=R.DE.B.l; }
static void ld_iyl_iyh(void) { R.IY.B.l=R.IY.B.h; }
static void ld_iyl_iyl(void) { }
static void ld_bc_xword(void) { R.BC.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_bc_word(void) { R.BC.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_de_xword(void) { R.DE.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_de_word(void) { R.DE.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_hl_xword(void) { R.HL.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_hl_word(void) { R.HL.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_ix_xword(void) { R.IX.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_ix_word(void) { R.IX.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_iy_xword(void) { R.IY.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_iy_word(void) { R.IY.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_sp_xword(void) { R.SP.W.l=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_sp_word(void) { R.SP.W.l=M_RDMEM_OPCODE_WORD(); }
static void ld_sp_hl(void) { R.SP.W.l=R.HL.W.l; }
static void ld_sp_ix(void) { R.SP.W.l=R.IX.W.l; }
static void ld_sp_iy(void) { R.SP.W.l=R.IY.W.l; }
static void ld_a_i(void)
{
 R.AF.B.h=R.I;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.I]|(R.IFF2<<2);
}
static void ld_i_a(void) { R.I=R.AF.B.h; }
static void ld_a_r(void)
{
 R.AF.B.h=(R.R&127)|(R.R2&128);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.AF.B.h]|(R.IFF2<<2);
}
static void ld_r_a(void) { R.R=R.R2=R.AF.B.h; }

static void ldd(void)
{
 M_WRMEM(R.DE.W.l,M_RDMEM(R.HL.W.l));
 --R.DE.W.l;
 --R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&0xE9)|(R.BC.W.l? V_FLAG:0);
}
static void lddr(void)
{
 ldd ();
 if (R.BC.W.l) { R.ICount+=5; R.PC.W.l-=2; }
}
static void ldi(void)
{
 M_WRMEM(R.DE.W.l,M_RDMEM(R.HL.W.l));
 ++R.DE.W.l;
 ++R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&0xE9)|(R.BC.W.l? V_FLAG:0);
}
static void ldir(void)
{
 ldi ();
 if (R.BC.W.l) { R.ICount+=5; R.PC.W.l-=2; }
}

static void neg(void)
{
 byte i;
 i=R.AF.B.h;
 R.AF.B.h=0;
 M_SUB(i);
}

static void nop(void) { };

static void or_xhl(void) { byte i=M_RD_XHL; M_OR(i); }
static void or_xix(void) { byte i=M_RD_XIX(); M_OR(i); }
static void or_xiy(void) { byte i=M_RD_XIY(); M_OR(i); }
static void or_a(void) { R.AF.B.l=ZSPTable[R.AF.B.h]; }
static void or_b(void) { M_OR(R.BC.B.h); }
static void or_c(void) { M_OR(R.BC.B.l); }
static void or_d(void) { M_OR(R.DE.B.h); }
static void or_e(void) { M_OR(R.DE.B.l); }
static void or_h(void) { M_OR(R.HL.B.h); }
static void or_l(void) { M_OR(R.HL.B.l); }
static void or_ixh(void) { M_OR(R.IX.B.h); }
static void or_ixl(void) { M_OR(R.IX.B.l); }
static void or_iyh(void) { M_OR(R.IY.B.h); }
static void or_iyl(void) { M_OR(R.IY.B.l); }
static void or_byte(void) { byte i=M_RDMEM_OPCODE(); M_OR(i); }

static void outd(void)
{
 --R.BC.B.h;
 DoOut (R.BC.B.l,R.BC.B.h,M_RDMEM(R.HL.W.l));
 --R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(Z_FLAG|N_FLAG);
}
static void otdr(void)
{
 outd ();
 if (R.BC.B.h) { R.ICount+=5; R.PC.W.l-=2; }
}
static void outi(void)
{
 --R.BC.B.h;
 DoOut (R.BC.B.l,R.BC.B.h,M_RDMEM(R.HL.W.l));
 ++R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(Z_FLAG|N_FLAG);
}
static void otir(void)
{
 outi ();
 if (R.BC.B.h) { R.ICount+=5; R.PC.W.l-=2; }
}

static void out_c_a(void) { DoOut(R.BC.B.l,R.BC.B.h,R.AF.B.h); }
static void out_c_b(void) { DoOut(R.BC.B.l,R.BC.B.h,R.BC.B.h); }
static void out_c_c(void) { DoOut(R.BC.B.l,R.BC.B.h,R.BC.B.l); }
static void out_c_d(void) { DoOut(R.BC.B.l,R.BC.B.h,R.DE.B.h); }
static void out_c_e(void) { DoOut(R.BC.B.l,R.BC.B.h,R.DE.B.l); }
static void out_c_h(void) { DoOut(R.BC.B.l,R.BC.B.h,R.HL.B.h); }
static void out_c_l(void) { DoOut(R.BC.B.l,R.BC.B.h,R.HL.B.l); }
static void out_c_0(void) { DoOut(R.BC.B.l,R.BC.B.h,0); }
static void out_byte_a(void)
{
 byte i=M_RDMEM_OPCODE();
 DoOut(i,R.AF.B.h,R.AF.B.h);
}

static void pop_af(void) { M_POP(AF); }
static void pop_bc(void) { M_POP(BC); }
static void pop_de(void) { M_POP(DE); }
static void pop_hl(void) { M_POP(HL); }
static void pop_ix(void) { M_POP(IX); }
static void pop_iy(void) { M_POP(IY); }

static void push_af(void) { M_PUSH(AF); }
static void push_bc(void) { M_PUSH(BC); }
static void push_de(void) { M_PUSH(DE); }
static void push_hl(void) { M_PUSH(HL); }
static void push_ix(void) { M_PUSH(IX); }
static void push_iy(void) { M_PUSH(IY); }

static void res_0_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(0,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_0_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(0,i);
 M_WRMEM(j,i);
};
static void res_0_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(0,i);
 M_WRMEM(j,i);
};
static void res_0_a(void) { M_RES(0,R.AF.B.h); };
static void res_0_b(void) { M_RES(0,R.BC.B.h); };
static void res_0_c(void) { M_RES(0,R.BC.B.l); };
static void res_0_d(void) { M_RES(0,R.DE.B.h); };
static void res_0_e(void) { M_RES(0,R.DE.B.l); };
static void res_0_h(void) { M_RES(0,R.HL.B.h); };
static void res_0_l(void) { M_RES(0,R.HL.B.l); };

static void res_1_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(1,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_1_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(1,i);
 M_WRMEM(j,i);
};
static void res_1_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(1,i);
 M_WRMEM(j,i);
};
static void res_1_a(void) { M_RES(1,R.AF.B.h); };
static void res_1_b(void) { M_RES(1,R.BC.B.h); };
static void res_1_c(void) { M_RES(1,R.BC.B.l); };
static void res_1_d(void) { M_RES(1,R.DE.B.h); };
static void res_1_e(void) { M_RES(1,R.DE.B.l); };
static void res_1_h(void) { M_RES(1,R.HL.B.h); };
static void res_1_l(void) { M_RES(1,R.HL.B.l); };

static void res_2_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(2,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_2_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(2,i);
 M_WRMEM(j,i);
};
static void res_2_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(2,i);
 M_WRMEM(j,i);
};
static void res_2_a(void) { M_RES(2,R.AF.B.h); };
static void res_2_b(void) { M_RES(2,R.BC.B.h); };
static void res_2_c(void) { M_RES(2,R.BC.B.l); };
static void res_2_d(void) { M_RES(2,R.DE.B.h); };
static void res_2_e(void) { M_RES(2,R.DE.B.l); };
static void res_2_h(void) { M_RES(2,R.HL.B.h); };
static void res_2_l(void) { M_RES(2,R.HL.B.l); };

static void res_3_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(3,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_3_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(3,i);
 M_WRMEM(j,i);
};
static void res_3_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(3,i);
 M_WRMEM(j,i);
};
static void res_3_a(void) { M_RES(3,R.AF.B.h); };
static void res_3_b(void) { M_RES(3,R.BC.B.h); };
static void res_3_c(void) { M_RES(3,R.BC.B.l); };
static void res_3_d(void) { M_RES(3,R.DE.B.h); };
static void res_3_e(void) { M_RES(3,R.DE.B.l); };
static void res_3_h(void) { M_RES(3,R.HL.B.h); };
static void res_3_l(void) { M_RES(3,R.HL.B.l); };

static void res_4_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(4,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_4_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(4,i);
 M_WRMEM(j,i);
};
static void res_4_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(4,i);
 M_WRMEM(j,i);
};
static void res_4_a(void) { M_RES(4,R.AF.B.h); };
static void res_4_b(void) { M_RES(4,R.BC.B.h); };
static void res_4_c(void) { M_RES(4,R.BC.B.l); };
static void res_4_d(void) { M_RES(4,R.DE.B.h); };
static void res_4_e(void) { M_RES(4,R.DE.B.l); };
static void res_4_h(void) { M_RES(4,R.HL.B.h); };
static void res_4_l(void) { M_RES(4,R.HL.B.l); };

static void res_5_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(5,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_5_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(5,i);
 M_WRMEM(j,i);
};
static void res_5_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(5,i);
 M_WRMEM(j,i);
};
static void res_5_a(void) { M_RES(5,R.AF.B.h); };
static void res_5_b(void) { M_RES(5,R.BC.B.h); };
static void res_5_c(void) { M_RES(5,R.BC.B.l); };
static void res_5_d(void) { M_RES(5,R.DE.B.h); };
static void res_5_e(void) { M_RES(5,R.DE.B.l); };
static void res_5_h(void) { M_RES(5,R.HL.B.h); };
static void res_5_l(void) { M_RES(5,R.HL.B.l); };

static void res_6_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(6,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_6_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(6,i);
 M_WRMEM(j,i);
};
static void res_6_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(6,i);
 M_WRMEM(j,i);
};
static void res_6_a(void) { M_RES(6,R.AF.B.h); };
static void res_6_b(void) { M_RES(6,R.BC.B.h); };
static void res_6_c(void) { M_RES(6,R.BC.B.l); };
static void res_6_d(void) { M_RES(6,R.DE.B.h); };
static void res_6_e(void) { M_RES(6,R.DE.B.l); };
static void res_6_h(void) { M_RES(6,R.HL.B.h); };
static void res_6_l(void) { M_RES(6,R.HL.B.l); };

static void res_7_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RES(7,i);
 M_WRMEM(R.HL.W.l,i);
};
static void res_7_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(7,i);
 M_WRMEM(j,i);
};
static void res_7_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(7,i);
 M_WRMEM(j,i);
};
static void res_7_a(void) { M_RES(7,R.AF.B.h); };
static void res_7_b(void) { M_RES(7,R.BC.B.h); };
static void res_7_c(void) { M_RES(7,R.BC.B.l); };
static void res_7_d(void) { M_RES(7,R.DE.B.h); };
static void res_7_e(void) { M_RES(7,R.DE.B.l); };
static void res_7_h(void) { M_RES(7,R.HL.B.h); };
static void res_7_l(void) { M_RES(7,R.HL.B.l); };

static void ret(void) { M_RET; }
static void ret_c(void) { if (M_C) { M_RET; } else { M_SKIP_RET; } }
static void ret_m(void) { if (M_M) { M_RET; } else { M_SKIP_RET; } }
static void ret_nc(void) { if (M_NC) { M_RET; } else { M_SKIP_RET; } }
static void ret_nz(void) { if (M_NZ) { M_RET; } else { M_SKIP_RET; } }
static void ret_p(void) { if (M_P) { M_RET; } else { M_SKIP_RET; } }
static void ret_pe(void) { if (M_PE) { M_RET; } else { M_SKIP_RET; } }
static void ret_po(void) { if (M_PO) { M_RET; } else { M_SKIP_RET; } }
static void ret_z(void) { if (M_Z) { M_RET; } else { M_SKIP_RET; } }

//static void reti(void) { Z80_Reti(); M_RET; }
//static void retn(void) { R.IFF1=R.IFF2; Z80_Retn(); M_RET; }
static void reti(void) { M_RET; }
static void retn(void) { R.IFF1=R.IFF2; M_RET; }

static void rl_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RL(i);
 M_WRMEM(R.HL.W.l,i);
}
static void rl_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RL(i);
 M_WRMEM(j,i);
}
static void rl_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RL(i);
 M_WRMEM(j,i);
}
static void rl_a(void) { M_RL(R.AF.B.h); }
static void rl_b(void) { M_RL(R.BC.B.h); }
static void rl_c(void) { M_RL(R.BC.B.l); }
static void rl_d(void) { M_RL(R.DE.B.h); }
static void rl_e(void) { M_RL(R.DE.B.l); }
static void rl_h(void) { M_RL(R.HL.B.h); }
static void rl_l(void) { M_RL(R.HL.B.l); }
static void rla(void)  { M_RLA; }

static void rlc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RLC(i);
 M_WRMEM(R.HL.W.l,i);
}
static void rlc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RLC(i);
 M_WRMEM(j,i);
}
static void rlc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RLC(i);
 M_WRMEM(j,i);
}
static void rlc_a(void) { M_RLC(R.AF.B.h); }
static void rlc_b(void) { M_RLC(R.BC.B.h); }
static void rlc_c(void) { M_RLC(R.BC.B.l); }
static void rlc_d(void) { M_RLC(R.DE.B.h); }
static void rlc_e(void) { M_RLC(R.DE.B.l); }
static void rlc_h(void) { M_RLC(R.HL.B.h); }
static void rlc_l(void) { M_RLC(R.HL.B.l); }
static void rlca(void)  { M_RLCA; }

static void rld(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_WRMEM(R.HL.W.l,(i<<4)|(R.AF.B.h&0x0F));
 R.AF.B.h=(R.AF.B.h&0xF0)|(i>>4);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rr_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RR(i);
 M_WRMEM(R.HL.W.l,i);
}
static void rr_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RR(i);
 M_WRMEM(j,i);
}
static void rr_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RR(i);
 M_WRMEM(j,i);
}
static void rr_a(void) { M_RR(R.AF.B.h); }
static void rr_b(void) { M_RR(R.BC.B.h); }
static void rr_c(void) { M_RR(R.BC.B.l); }
static void rr_d(void) { M_RR(R.DE.B.h); }
static void rr_e(void) { M_RR(R.DE.B.l); }
static void rr_h(void) { M_RR(R.HL.B.h); }
static void rr_l(void) { M_RR(R.HL.B.l); }
static void rra(void)  { M_RRA; }

static void rrc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_RRC(i);
 M_WRMEM(R.HL.W.l,i);
}
static void rrc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RRC(i);
 M_WRMEM(j,i);
}
static void rrc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RRC(i);
 M_WRMEM(j,i);
}
static void rrc_a(void) { M_RRC(R.AF.B.h); }
static void rrc_b(void) { M_RRC(R.BC.B.h); }
static void rrc_c(void) { M_RRC(R.BC.B.l); }
static void rrc_d(void) { M_RRC(R.DE.B.h); }
static void rrc_e(void) { M_RRC(R.DE.B.l); }
static void rrc_h(void) { M_RRC(R.HL.B.h); }
static void rrc_l(void) { M_RRC(R.HL.B.l); }
static void rrca(void)  { M_RRCA; }

static void rrd(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_WRMEM(R.HL.W.l,(i>>4)|(R.AF.B.h<<4));
 R.AF.B.h=(R.AF.B.h&0xF0)|(i&0x0F);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rst_00(void) { M_RST(0x00); }
static void rst_08(void) { M_RST(0x08); }
static void rst_10(void) { M_RST(0x10); }
static void rst_18(void) { M_RST(0x18); }
static void rst_20(void) { M_RST(0x20); }
static void rst_28(void) { M_RST(0x28); }
static void rst_30(void) { M_RST(0x30); }
static void rst_38(void) { M_RST(0x38); }

static void sbc_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_SBC(i); }
static void sbc_a_xhl(void) { byte i=M_RD_XHL; M_SBC(i); }
static void sbc_a_xix(void) { byte i=M_RD_XIX(); M_SBC(i); }
static void sbc_a_xiy(void) { byte i=M_RD_XIY(); M_SBC(i); }
static void sbc_a_a(void) { M_SBC(R.AF.B.h); }
static void sbc_a_b(void) { M_SBC(R.BC.B.h); }
static void sbc_a_c(void) { M_SBC(R.BC.B.l); }
static void sbc_a_d(void) { M_SBC(R.DE.B.h); }
static void sbc_a_e(void) { M_SBC(R.DE.B.l); }
static void sbc_a_h(void) { M_SBC(R.HL.B.h); }
static void sbc_a_l(void) { M_SBC(R.HL.B.l); }
static void sbc_a_ixh(void) { M_SBC(R.IX.B.h); }
static void sbc_a_ixl(void) { M_SBC(R.IX.B.l); }
static void sbc_a_iyh(void) { M_SBC(R.IY.B.h); }
static void sbc_a_iyl(void) { M_SBC(R.IY.B.l); }

static void sbc_hl_bc(void) { M_SBCW(BC); }
static void sbc_hl_de(void) { M_SBCW(DE); }
static void sbc_hl_hl(void) { M_SBCW(HL); }
static void sbc_hl_sp(void) { M_SBCW(SP); }

static void scf(void) { R.AF.B.l=(R.AF.B.l&0xEC)|C_FLAG; }

static void set_0_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(0,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_0_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(0,i);
 M_WRMEM(j,i);
};
static void set_0_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(0,i);
 M_WRMEM(j,i);
};
static void set_0_a(void) { M_SET(0,R.AF.B.h); };
static void set_0_b(void) { M_SET(0,R.BC.B.h); };
static void set_0_c(void) { M_SET(0,R.BC.B.l); };
static void set_0_d(void) { M_SET(0,R.DE.B.h); };
static void set_0_e(void) { M_SET(0,R.DE.B.l); };
static void set_0_h(void) { M_SET(0,R.HL.B.h); };
static void set_0_l(void) { M_SET(0,R.HL.B.l); };

static void set_1_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(1,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_1_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(1,i);
 M_WRMEM(j,i);
};
static void set_1_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(1,i);
 M_WRMEM(j,i);
};
static void set_1_a(void) { M_SET(1,R.AF.B.h); };
static void set_1_b(void) { M_SET(1,R.BC.B.h); };
static void set_1_c(void) { M_SET(1,R.BC.B.l); };
static void set_1_d(void) { M_SET(1,R.DE.B.h); };
static void set_1_e(void) { M_SET(1,R.DE.B.l); };
static void set_1_h(void) { M_SET(1,R.HL.B.h); };
static void set_1_l(void) { M_SET(1,R.HL.B.l); };

static void set_2_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(2,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_2_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(2,i);
 M_WRMEM(j,i);
};
static void set_2_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(2,i);
 M_WRMEM(j,i);
};
static void set_2_a(void) { M_SET(2,R.AF.B.h); };
static void set_2_b(void) { M_SET(2,R.BC.B.h); };
static void set_2_c(void) { M_SET(2,R.BC.B.l); };
static void set_2_d(void) { M_SET(2,R.DE.B.h); };
static void set_2_e(void) { M_SET(2,R.DE.B.l); };
static void set_2_h(void) { M_SET(2,R.HL.B.h); };
static void set_2_l(void) { M_SET(2,R.HL.B.l); };

static void set_3_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(3,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_3_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(3,i);
 M_WRMEM(j,i);
};
static void set_3_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(3,i);
 M_WRMEM(j,i);
};
static void set_3_a(void) { M_SET(3,R.AF.B.h); };
static void set_3_b(void) { M_SET(3,R.BC.B.h); };
static void set_3_c(void) { M_SET(3,R.BC.B.l); };
static void set_3_d(void) { M_SET(3,R.DE.B.h); };
static void set_3_e(void) { M_SET(3,R.DE.B.l); };
static void set_3_h(void) { M_SET(3,R.HL.B.h); };
static void set_3_l(void) { M_SET(3,R.HL.B.l); };

static void set_4_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(4,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_4_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(4,i);
 M_WRMEM(j,i);
};
static void set_4_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(4,i);
 M_WRMEM(j,i);
};
static void set_4_a(void) { M_SET(4,R.AF.B.h); };
static void set_4_b(void) { M_SET(4,R.BC.B.h); };
static void set_4_c(void) { M_SET(4,R.BC.B.l); };
static void set_4_d(void) { M_SET(4,R.DE.B.h); };
static void set_4_e(void) { M_SET(4,R.DE.B.l); };
static void set_4_h(void) { M_SET(4,R.HL.B.h); };
static void set_4_l(void) { M_SET(4,R.HL.B.l); };

static void set_5_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(5,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_5_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(5,i);
 M_WRMEM(j,i);
};
static void set_5_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(5,i);
 M_WRMEM(j,i);
};
static void set_5_a(void) { M_SET(5,R.AF.B.h); };
static void set_5_b(void) { M_SET(5,R.BC.B.h); };
static void set_5_c(void) { M_SET(5,R.BC.B.l); };
static void set_5_d(void) { M_SET(5,R.DE.B.h); };
static void set_5_e(void) { M_SET(5,R.DE.B.l); };
static void set_5_h(void) { M_SET(5,R.HL.B.h); };
static void set_5_l(void) { M_SET(5,R.HL.B.l); };

static void set_6_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(6,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_6_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(6,i);
 M_WRMEM(j,i);
};
static void set_6_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(6,i);
 M_WRMEM(j,i);
};
static void set_6_a(void) { M_SET(6,R.AF.B.h); };
static void set_6_b(void) { M_SET(6,R.BC.B.h); };
static void set_6_c(void) { M_SET(6,R.BC.B.l); };
static void set_6_d(void) { M_SET(6,R.DE.B.h); };
static void set_6_e(void) { M_SET(6,R.DE.B.l); };
static void set_6_h(void) { M_SET(6,R.HL.B.h); };
static void set_6_l(void) { M_SET(6,R.HL.B.l); };

static void set_7_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SET(7,i);
 M_WRMEM(R.HL.W.l,i);
};
static void set_7_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(7,i);
 M_WRMEM(j,i);
};
static void set_7_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(7,i);
 M_WRMEM(j,i);
};
static void set_7_a(void) { M_SET(7,R.AF.B.h); };
static void set_7_b(void) { M_SET(7,R.BC.B.h); };
static void set_7_c(void) { M_SET(7,R.BC.B.l); };
static void set_7_d(void) { M_SET(7,R.DE.B.h); };
static void set_7_e(void) { M_SET(7,R.DE.B.l); };
static void set_7_h(void) { M_SET(7,R.HL.B.h); };
static void set_7_l(void) { M_SET(7,R.HL.B.l); };

static void sla_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SLA(i);
 M_WRMEM(R.HL.W.l,i);
}
static void sla_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SLA(i);
 M_WRMEM(j,i);
}
static void sla_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SLA(i);
 M_WRMEM(j,i);
}
static void sla_a(void) { M_SLA(R.AF.B.h); }
static void sla_b(void) { M_SLA(R.BC.B.h); }
static void sla_c(void) { M_SLA(R.BC.B.l); }
static void sla_d(void) { M_SLA(R.DE.B.h); }
static void sla_e(void) { M_SLA(R.DE.B.l); }
static void sla_h(void) { M_SLA(R.HL.B.h); }
static void sla_l(void) { M_SLA(R.HL.B.l); }

static void sll_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SLL(i);
 M_WRMEM(R.HL.W.l,i);
}
static void sll_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SLL(i);
 M_WRMEM(j,i);
}
static void sll_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SLL(i);
 M_WRMEM(j,i);
}
static void sll_a(void) { M_SLL(R.AF.B.h); }
static void sll_b(void) { M_SLL(R.BC.B.h); }
static void sll_c(void) { M_SLL(R.BC.B.l); }
static void sll_d(void) { M_SLL(R.DE.B.h); }
static void sll_e(void) { M_SLL(R.DE.B.l); }
static void sll_h(void) { M_SLL(R.HL.B.h); }
static void sll_l(void) { M_SLL(R.HL.B.l); }

static void sra_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SRA(i);
 M_WRMEM(R.HL.W.l,i);
}
static void sra_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SRA(i);
 M_WRMEM(j,i);
}
static void sra_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SRA(i);
 M_WRMEM(j,i);
}
static void sra_a(void) { M_SRA(R.AF.B.h); }
static void sra_b(void) { M_SRA(R.BC.B.h); }
static void sra_c(void) { M_SRA(R.BC.B.l); }
static void sra_d(void) { M_SRA(R.DE.B.h); }
static void sra_e(void) { M_SRA(R.DE.B.l); }
static void sra_h(void) { M_SRA(R.HL.B.h); }
static void sra_l(void) { M_SRA(R.HL.B.l); }

static void srl_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.W.l);
 M_SRL(i);
 M_WRMEM(R.HL.W.l,i);
}
static void srl_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SRL(i);
 M_WRMEM(j,i);
}
static void srl_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SRL(i);
 M_WRMEM(j,i);
}
static void srl_a(void) { M_SRL(R.AF.B.h); }
static void srl_b(void) { M_SRL(R.BC.B.h); }
static void srl_c(void) { M_SRL(R.BC.B.l); }
static void srl_d(void) { M_SRL(R.DE.B.h); }
static void srl_e(void) { M_SRL(R.DE.B.l); }
static void srl_h(void) { M_SRL(R.HL.B.h); }
static void srl_l(void) { M_SRL(R.HL.B.l); }

static void sub_xhl(void) { byte i=M_RD_XHL; M_SUB(i); }
static void sub_xix(void) { byte i=M_RD_XIX(); M_SUB(i); }
static void sub_xiy(void) { byte i=M_RD_XIY(); M_SUB(i); }
static void sub_a(void) { R.AF.W.l=Z_FLAG|N_FLAG; }
static void sub_b(void) { M_SUB(R.BC.B.h); }
static void sub_c(void) { M_SUB(R.BC.B.l); }
static void sub_d(void) { M_SUB(R.DE.B.h); }
static void sub_e(void) { M_SUB(R.DE.B.l); }
static void sub_h(void) { M_SUB(R.HL.B.h); }
static void sub_l(void) { M_SUB(R.HL.B.l); }
static void sub_ixh(void) { M_SUB(R.IX.B.h); }
static void sub_ixl(void) { M_SUB(R.IX.B.l); }
static void sub_iyh(void) { M_SUB(R.IY.B.h); }
static void sub_iyl(void) { M_SUB(R.IY.B.l); }
static void sub_byte(void) { byte i=M_RDMEM_OPCODE(); M_SUB(i); }

static void xor_xhl(void) { byte i=M_RD_XHL; M_XOR(i); }
static void xor_xix(void) { byte i=M_RD_XIX(); M_XOR(i); }
static void xor_xiy(void) { byte i=M_RD_XIY(); M_XOR(i); }
static void xor_a(void) { R.AF.W.l=Z_FLAG|V_FLAG;}
static void xor_b(void) { M_XOR(R.BC.B.h); }
static void xor_c(void) { M_XOR(R.BC.B.l); }
static void xor_d(void) { M_XOR(R.DE.B.h); }
static void xor_e(void) { M_XOR(R.DE.B.l); }
static void xor_h(void) { M_XOR(R.HL.B.h); }
static void xor_l(void) { M_XOR(R.HL.B.l); }
static void xor_ixh(void) { M_XOR(R.IX.B.h); }
static void xor_ixl(void) { M_XOR(R.IX.B.l); }
static void xor_iyh(void) { M_XOR(R.IY.B.h); }
static void xor_iyl(void) { M_XOR(R.IY.B.l); }
static void xor_byte(void) { byte i=M_RDMEM_OPCODE(); M_XOR(i); }

static void no_op(void)
{
 --R.PC.W.l;
}

#define patch nop
//static void patch(void) { Z80_Patch(R); }

static void dd_cb(void);
static void fd_cb(void);
static void cb(void);
static void dd(void);
static void ed(void);
static void fd(void);
static void ei(void);
static void no_op_xx(void) {
++R.PC.W.l; }
 

//#include "opc_R800.h"
#include "opc__Z80.h"
static void dd_cb(void)
{
 unsigned opcode;
 opcode=M_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
 R.ICount+=cycles_xx_cb[opcode];
 (*(opcode_dd_cb[opcode]))();
 ++R.PC.W.l;
};
static void fd_cb(void)
{
 unsigned opcode;
 opcode=M_RDOP_ARG((R.PC.W.l+1)&0xFFFF);
 R.ICount+=cycles_xx_cb[opcode];
 (*(opcode_fd_cb[opcode]))();
 ++R.PC.W.l;
};

static void cb(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.W.l);
 R.PC.W.l++;
 R.ICount+=cycles_cb[opcode];
 (*(opcode_cb[opcode]))();
}
static void dd(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.W.l);
 R.PC.W.l++;
 R.ICount+=cycles_xx[opcode];
 (*(opcode_dd[opcode]))();
}
static void ed(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.W.l);
 R.PC.W.l++;
 R.ICount+=cycles_ed[opcode];
 (*(opcode_ed[opcode]))();
}
static void fd (void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.W.l);
 R.PC.W.l++;
 R.ICount+=cycles_xx[opcode];
 (*(opcode_fd[opcode]))();
}

static void ei(void)
{
 unsigned opcode;
 /* If interrupts were disabled, execute one more instruction and check the */
 /* IRQ line. If not, simply set interrupt flip-flop 2                      */
 if (!R.IFF1)
 {
#ifdef DEBUG
//  if (R.PC.W.l==Z80_Trap) Z80_Trace=1;
//  if (Z80_Trace) Z80_Debug(&R);
#endif
  R.IFF1=R.IFF2=1;
  ++R.R;
  opcode=M_RDOP(R.PC.W.l);
  R.PC.W.l++;
  R.ICount+=cycles_main[opcode];
  (*(opcode_main[opcode]))(); // Gives wrong timing for the moment !!
  Interrupt(Z80_IRQ);
 }
 else
  R.IFF2=1;
}

/****************************************************************************/
/* Reset registers to their initial values                                  */
/****************************************************************************/
/*void Z80_Reset (void)
{
 memset (&R,0,sizeof(CPU_Regs));
 R.SP.W.l=0xF000;
 R.R=rand();
// R.ICount=R.IPeriod;
}
*/
/****************************************************************************/
/* Initialise the various lookup tables used by the emulation code          */
/****************************************************************************/
void InitTables (void)
{
 static int InitTables_virgin=1;
 byte zs;
 int i,p;
 if (!InitTables_virgin) return;
 InitTables_virgin=0;
 for (i=0;i<256;++i)
 {
  zs=0;
  if (i==0)
   zs|=Z_FLAG;
  if (i&0x80)
   zs|=S_FLAG;
  p=0;
  if (i&1) ++p;
  if (i&2) ++p;
  if (i&4) ++p;
  if (i&8) ++p;
  if (i&16) ++p;
  if (i&32) ++p;
  if (i&64) ++p;
  if (i&128) ++p;
  PTable[i]=(p&1)? 0:V_FLAG;
  ZSTable[i]=zs;
  ZSPTable[i]=zs|PTable[i];
 }
 for (i=0;i<256;++i)
 {
  ZSTable[i+256]=ZSTable[i]|C_FLAG;
  ZSPTable[i+256]=ZSPTable[i]|C_FLAG;
  PTable[i+256]=PTable[i]|C_FLAG;
 }
}

/****************************************************************************/
/* Issue an interrupt if necessary                                          */
/****************************************************************************/
//Since now needed by the scheduler it is no longer static
void Interrupt (int j)
{
 printf("In interrupt routine:\n");
 printf("if (j==Z80_IGNORE_INT) return; %i\n", (j==Z80_IGNORE_INT));
 printf("if (j==Z80_NMI_INT || R.IFF1) %i || %i \n", (j==Z80_NMI_INT),R.IFF1);
 if (j==Z80_IGNORE_INT) return;
 if (j==Z80_NMI_INT || R.IFF1)
 {
  /* Clear interrupt flip-flop 1 */
 printf("  R.IFF1 : %i ",R.IFF1 );
  R.IFF1=0;
 printf("  R.IFF1 : %i R.IM : %i \n",R.IFF1,R.IM );
  /* Check if processor was halted */
  if (R.HALT)
  {
   ++R.PC.W.l;
   R.HALT=0;
  }
  if (j==Z80_NMI_INT)
  {
   M_PUSH (PC);
   R.PC.W.l=0x0066;
  }
  else
  {
   /* Interrupt mode 2. Call [R.I:databyte] */
   if (R.IM==2)
   {
    M_PUSH (PC);
    R.PC.W.l=M_RDMEM_WORD((j&255)|(R.I<<8));
   }
   else
    /* Interrupt mode 1. RST 38h */
    if (R.IM==1)
    {
     R.ICount+=cycles_main[0xFF];
     (*(opcode_main[0xFF]))();
    }
    else
    /* Interrupt mode 0. We check for CALL and JP instructions, if neither  */
    /* of these were found we assume a 1 byte opcode was placed on the      */
    /* databus                                                              */
    {
     switch (j&0xFF0000)
     {
      case 0xCD:
       M_PUSH(PC);
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
/* Return program counter                                                   */
/****************************************************************************/
unsigned Z80_GetPC (void)
{
 return R.PC.W.l;
}

/****************************************************************************/
/* Execute IPeriod T-States. Return 0 if emulation should be stopped        */
/****************************************************************************/
int Z80_SingleInstruction(void)
{
 static unsigned opcode;
#ifdef DEBUG
 word start_pc;
// char* to_print_string="                            ";
#endif
 // tijdelijk static voor debugger: unsigned opcode;
 /*
  * Z80_Running=1;
  * InitTables ();
  * moved to MSXZ80.cpp
  */

//  printf(" R.PC.W.l : %04x\n",R.PC.W.l);
  if (R.PC.W.l == 0) {printf(" RESET PC           \n");};
#ifdef TRACE
  pc_trace[pc_count]=R.PC.W.l;
  pc_count=(pc_count+1)&255;
#endif
#ifdef DEBUG
  start_pc=R.PC.W.l;
  printf("%04x : ",start_pc);
//  if (R.PC.W.l==R.Trap) R.Trace=1;
//  if (Z80_Trace) Z80_Debug(&R);
//  if (!Z80_Running) break;
#endif
  ++R.R;
  opcode=M_RDOP(R.PC.W.l);
  R.PC.W.l++;
  //R.ICount+=cycles_main[opcode];
  R.ICount=cycles_main[opcode]; // instead of R.Icount=0
  (*(opcode_main[opcode]))();
/*  CurrentCPUTime+=R.ICount; // During prev line R.ICount can be raised extra
	must be in MSXZ80 class!!!
    */
    //R.ICount=0;
//  TODO: still need to adapt all other code to change the CurrentCPUTime 
 // Moved to MSXZ80 Interrupt (Z80_Interrupt());
#ifdef DEBUG
  printf("instruction ");
  Z80_Dasm(&debugmemory[start_pc], to_print_string,start_pc );
  printf("%s\n",to_print_string );
  printf("      A=%02x F=%02x \n",R.AF.B.h,R.AF.B.l);
  printf("      BC=%04x DE=%04x HL=%04x \n",R.BC.W.l,R.DE.W.l,R.HL.W.l);
#endif
 return Z80_Running;
}

/****************************************************************************/
/* Interpret Z80 code                                                       */
/****************************************************************************/
//word Z80 (void)
//{
 //while (Z80_Execute());
 //return(R.PC.W.l);
//}

/****************************************************************************/
/* Dump register contents and (optionally) a PC trace to stdout             */
/****************************************************************************/
void Z80_RegisterDump (void)
{
 int i;
 printf
 (
   "AF:%04X HL:%04X DE:%04X BC:%04X PC:%04X SP:%04X IX:%04X IY:%04X\n",
   R.AF.W.l,R.HL.W.l,R.DE.W.l,R.BC.W.l,R.PC.W.l,R.SP.W.l,R.IX.W.l,R.IY.W.l
 ); 
 printf ("STACK: ");
 for (i=0;i<10;++i) printf ("%04X ",M_RDMEM_WORD((R.SP.W.l+i*2)&0xFFFF));
 puts ("");
#ifdef TRACE
 puts ("PC TRACE:");
 for (i=1;i<=256;++i) printf ("%04X\n",pc_trace[(pc_count-i)&255]);
#endif
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80_SetWaitStates (int n)
{
 int i;
 for (i=0;i<256;++i)
 {
  cycles_main[i]+=n;
  cycles_cb[i]+=n;
  cycles_ed[i]+=n;
  cycles_xx[i]+=n;
 }
}

/****************************************************************************/
/* Switch between the two  modes : R800 and Z80                             */
/****************************************************************************/
//static int currentCPU=3;
//void set_Z80(void)
//{
//	if (!currentCPU){
//		printf("!currentCPU\n");
//		return;
//	};
//	currentCPU=0;
//	printf("&R %xi\n",(int)&R);
//	printf("&R_Z80 %xi\n",(int)&R_Z80);
//	R=&R_Z80;
//	printf("&R %x\n",(int)&R);
//	printf("&R_Z80 %x\n",(int)&R_Z80);
//	cycles_main	= Z80_cycles_main;
//	cycles_cb	= Z80_cycles_cb;
//	cycles_xx_cb	= Z80_cycles_xx_cb;
//	cycles_xx	= Z80_cycles_xx;
//	cycles_ed	= Z80_cycles_ed;
//	opcode_dd_cb	= Z80_opcode_dd_cb;
//	opcode_fd_cb	= Z80_opcode_fd_cb;
//	opcode_cb	= Z80_opcode_cb;
//	opcode_dd	= Z80_opcode_dd;
//	opcode_ed	= Z80_opcode_ed;
//	opcode_fd	= Z80_opcode_fd;
//	opcode_main	= Z80_opcode_main;
//}
//
//void set_R800(void)
//{
//	if (currentCPU){
//		return;
//	};
//	currentCPU=1;
//	R=&R_R800;
//	cycles_main	= R800_cycles_main;
//	cycles_cb	= R800_cycles_cb;
//	cycles_xx_cb	= R800_cycles_xx_cb;
//	cycles_xx	= R800_cycles_xx;
//	cycles_ed	= R800_cycles_ed;
//	opcode_dd_cb	= R800_opcode_dd_cb;
//	opcode_fd_cb	= R800_opcode_fd_cb;
//	opcode_cb	= R800_opcode_cb;
//	opcode_dd	= R800_opcode_dd;
//	opcode_ed	= R800_opcode_ed;
//	opcode_fd	= R800_opcode_fd;
//	opcode_main	= R800_opcode_main;
//}

/****************************************************************************/
/* Reset the CPU emulation core                                             */
/* Set registers to their initial values                                    */
/* and make the Z80 the starting CPU                                        */
/****************************************************************************/
void Reset_CPU (void)
{
	//Clear all data
	memset (&R,0,sizeof(CPU_Regs));
	R.SP.W.l=0xF000;
	R.IX.W.l=0xFFFF;
	R.IY.W.l=0xFFFF;
	R.R=(byte)rand();
//When this was both R800 and Z80 this code was usefull
//	//Clear all Z80 data
//	memset (&R_Z80,0,sizeof(CPU_Regs));
//	R_Z80.SP.D=0xF000;
//	R_Z80.IX.D=0xFFFF;
//	R_Z80.IY.D=0xFFFF;
//	R_Z80.R=rand();
//	//Clear all R800 data
//	memset (&R_R800,0,sizeof(CPU_Regs));
//	R_R800.SP.D=0xF000;
//	R_R800.IX.D=0xFFFF;
//	R_R800.IY.D=0xFFFF;
//	R_R800.R=rand();
//	//Set current CPU to Z80
//	set_Z80();
}

/****************************************************************************/
/* Set the number of Iperiods into both the R800 and Z80                    */
/* The number passed to this function is the number of iperiods for an      */
/* 28 MHz clock. R800 and Z80 run on a different clock.                     */
/****************************************************************************/
//void set_IPeriod(int period)
//{
//	int R800_TimeDivide=4; //= 28 /  7
//	int Z80_TimeDivide=8; //=  28 / 3.5
//
//	R_Z80.ICount=R_Z80.IPeriod=period/Z80_TimeDivide;
//	R_R800.ICount=R_R800.IPeriod=period/R800_TimeDivide;
//	printf("R_Z80.ICount=%i\n",R_Z80.ICount);
//	printf("R_Z80.IPeriod=%i\n",R_Z80.IPeriod);
//	printf("R_R800.ICount=%i\n",R_R800.ICount);
//	printf("R_R800.IPeriod=%i\n\n",R_R800.IPeriod);
//	
//	printf("R.ICount=%i\n",R.ICount);
//	printf("R.IPeriod=%i\n\n",R.IPeriod);
//}
//
//void reset_IPeriod(void)
//{
//	R.ICount=R.IPeriod;
//
//}
