// $Id$

// TODO update heading
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

#include <cassert>

#include "Z80.hh"

#ifdef Z80DEBUG
#include <stdio.h>
#include "Z80Dasm.h"
#endif

#include "Z80Tables.hh"
#include "Z80.ii"

Z80::Z80(CPUInterface *interf, int waitCycl, const EmuTime &time) : CPU(interf)
{
	for (int i=0; i<256; ++i) {
		byte zFlag = (i==0) ? Z_FLAG : 0;
		byte sFlag = i & S_FLAG;
		byte xFlag = i & X_FLAG;
		byte yFlag = i & Y_FLAG;
		int p = 0;
		if (i&1)   ++p;
		if (i&2)   ++p;
		if (i&4)   ++p;
		if (i&8)   ++p;
		if (i&16)  ++p;
		if (i&32)  ++p;
		if (i&64)  ++p;
		if (i&128) ++p;
		byte pFlag = (p&1) ? 0 : V_FLAG;
		ZSTable[i]    = zFlag | sFlag;
		XYTable[i]    =                 xFlag | yFlag;
		ZSXYTable[i]  = zFlag | sFlag | xFlag | yFlag;
		ZSPXYTable[i] = zFlag | sFlag | xFlag | yFlag | pFlag;
	}
	waitCycles = waitCycl;
}
Z80::~Z80()
{
}

void Z80::setCurrentTime(const EmuTime &time)
{
	currentTime = time;
}
const EmuTime &Z80::getCurrentTime()
{
	return currentTime;
}

/****************************************************************************/
/* Reset the CPU emulation core                                             */
/* Set registers to their initial values                                    */
/****************************************************************************/
void Z80::reset(const EmuTime &time)
{
	// AF and SP are 0xffff
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	R.AF.w = 0xffff;
	R.BC.w = 0xffff;
	R.DE.w = 0xffff;
	R.HL.w = 0xffff;
	R.IX.w = 0xffff;
	R.IY.w = 0xffff;
	R.PC.w = 0x0000;
	R.SP.w = 0xffff;
	
	R.AF2.w = 0xffff;
	R.BC2.w = 0xffff;
	R.DE2.w = 0xffff;
	R.HL2.w = 0xffff;

	R.nextIFF1 = false;
	R.IFF1 = false;
	R.IFF2 = false;
	R.HALT = false;
	
	R.IM = 0;
	R.I = 0xff;
	R.R = 0x00;
	R.R2 = 0;

	setCurrentTime(time);
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/


void Z80::execute()
{
	while (currentTime < targetTime) {
		if (interface->NMIEdge()) { 
			// NMI occured
			R.HALT = false; 
			R.IFF1 = R.nextIFF1 = false;
			M_PUSH (R.PC.w);
			R.PC.w=0x0066;
			M1Cycle();
			currentTime += 11;
		} else if (R.IFF1 && interface->IRQStatus()) {
			// normal interrupt
			R.HALT = false;
			R.IFF1 = R.nextIFF1 = false;
			switch (R.IM) {
			case 2:
				// Interrupt mode 2  Call [I:databyte]
				M_PUSH (R.PC.w);
				R.PC.w=Z80_RDMEM_WORD((interface->dataBus())|(R.I<<8));
				M1Cycle();
				currentTime += 19;
				break;
			case 1:
				// Interrupt mode 1
				currentTime += 2;
				executeInstruction(0xff);	// RST 38h
				break;
			case 0:
				// Interrupt mode 0
				// TODO current implementation only works for 1-byte instructions
				//      ok for MSX
				currentTime += 2;
				executeInstruction(interface->dataBus());
				break;
			default:
				assert(false);
			}
		} else if (R.HALT) {
			// in halt mode
			const int haltStates = 4 + waitCycles;	// HALT + M1
			uint64 ticks = currentTime.getTicksTill(targetTime);
			int halts = (ticks+(haltStates-1))/haltStates;	// rounded up
			R.R += halts;
			currentTime += halts*haltStates;
		} else {
			// normal instructions
			targetChanged = false;
			while (!targetChanged && (currentTime < targetTime)) {
				executeInstruction(Z80_RDOP(R.PC.w++));
			}
		}
	}
}

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

void Z80::adc_hl_bc() { M_ADCW(R.BC.w); }
void Z80::adc_hl_de() { M_ADCW(R.DE.w); }
void Z80::adc_hl_hl() { M_ADCW(R.HL.w); }
void Z80::adc_hl_sp() { M_ADCW(R.SP.w); }

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

void Z80::add_hl_bc() { M_ADDW(R.HL.w, R.BC.w); }
void Z80::add_hl_de() { M_ADDW(R.HL.w, R.DE.w); }
void Z80::add_hl_hl() { M_ADDW(R.HL.w, R.HL.w); }
void Z80::add_hl_sp() { M_ADDW(R.HL.w, R.SP.w); }
void Z80::add_ix_bc() { M_ADDW(R.IX.w, R.BC.w); }
void Z80::add_ix_de() { M_ADDW(R.IX.w, R.DE.w); }
void Z80::add_ix_ix() { M_ADDW(R.IX.w, R.IX.w); }
void Z80::add_ix_sp() { M_ADDW(R.IX.w, R.SP.w); }
void Z80::add_iy_bc() { M_ADDW(R.IY.w, R.BC.w); }
void Z80::add_iy_de() { M_ADDW(R.IY.w, R.DE.w); }
void Z80::add_iy_iy() { M_ADDW(R.IY.w, R.IY.w); }
void Z80::add_iy_sp() { M_ADDW(R.IY.w, R.SP.w); }

void Z80::and_xhl() { M_AND(M_RD_XHL()); }
void Z80::and_xix() { M_AND(M_RD_XIX()); }
void Z80::and_xiy() { M_AND(M_RD_XIY()); }
void Z80::and_a()   { R.AF.B.l = ZSPXYTable[R.AF.B.h]|H_FLAG; }
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

void Z80::bit_0_xhl() { M_BIT(0, M_RD_XHL()); currentTime++; }
void Z80::bit_0_xix() { M_BIT(0, M_RD_XIX()); currentTime++; }
void Z80::bit_0_xiy() { M_BIT(0, M_RD_XIY()); currentTime++; }
void Z80::bit_0_a()   { M_BIT(0, R.AF.B.h); }
void Z80::bit_0_b()   { M_BIT(0, R.BC.B.h); }
void Z80::bit_0_c()   { M_BIT(0, R.BC.B.l); }
void Z80::bit_0_d()   { M_BIT(0, R.DE.B.h); }
void Z80::bit_0_e()   { M_BIT(0, R.DE.B.l); }
void Z80::bit_0_h()   { M_BIT(0, R.HL.B.h); }
void Z80::bit_0_l()   { M_BIT(0, R.HL.B.l); }

void Z80::bit_1_xhl() { M_BIT(1, M_RD_XHL()); currentTime++; }
void Z80::bit_1_xix() { M_BIT(1, M_RD_XIX()); currentTime++; }
void Z80::bit_1_xiy() { M_BIT(1, M_RD_XIY()); currentTime++; }
void Z80::bit_1_a()   { M_BIT(1, R.AF.B.h); }
void Z80::bit_1_b()   { M_BIT(1, R.BC.B.h); }
void Z80::bit_1_c()   { M_BIT(1, R.BC.B.l); }
void Z80::bit_1_d()   { M_BIT(1, R.DE.B.h); }
void Z80::bit_1_e()   { M_BIT(1, R.DE.B.l); }
void Z80::bit_1_h()   { M_BIT(1, R.HL.B.h); }
void Z80::bit_1_l()   { M_BIT(1, R.HL.B.l); }

void Z80::bit_2_xhl() { M_BIT(2, M_RD_XHL()); currentTime++; }
void Z80::bit_2_xix() { M_BIT(2, M_RD_XIX()); currentTime++; }
void Z80::bit_2_xiy() { M_BIT(2, M_RD_XIY()); currentTime++; }
void Z80::bit_2_a()   { M_BIT(2, R.AF.B.h); }
void Z80::bit_2_b()   { M_BIT(2, R.BC.B.h); }
void Z80::bit_2_c()   { M_BIT(2, R.BC.B.l); }
void Z80::bit_2_d()   { M_BIT(2, R.DE.B.h); }
void Z80::bit_2_e()   { M_BIT(2, R.DE.B.l); }
void Z80::bit_2_h()   { M_BIT(2, R.HL.B.h); }
void Z80::bit_2_l()   { M_BIT(2, R.HL.B.l); }

void Z80::bit_3_xhl() { M_BIT(3, M_RD_XHL()); currentTime++; }
void Z80::bit_3_xix() { M_BIT(3, M_RD_XIX()); currentTime++; }
void Z80::bit_3_xiy() { M_BIT(3, M_RD_XIY()); currentTime++; }
void Z80::bit_3_a()   { M_BIT(3, R.AF.B.h); }
void Z80::bit_3_b()   { M_BIT(3, R.BC.B.h); }
void Z80::bit_3_c()   { M_BIT(3, R.BC.B.l); }
void Z80::bit_3_d()   { M_BIT(3, R.DE.B.h); }
void Z80::bit_3_e()   { M_BIT(3, R.DE.B.l); }
void Z80::bit_3_h()   { M_BIT(3, R.HL.B.h); }
void Z80::bit_3_l()   { M_BIT(3, R.HL.B.l); }

void Z80::bit_4_xhl() { M_BIT(4, M_RD_XHL()); currentTime++; }
void Z80::bit_4_xix() { M_BIT(4, M_RD_XIX()); currentTime++; }
void Z80::bit_4_xiy() { M_BIT(4, M_RD_XIY()); currentTime++; }
void Z80::bit_4_a()   { M_BIT(4, R.AF.B.h); }
void Z80::bit_4_b()   { M_BIT(4, R.BC.B.h); }
void Z80::bit_4_c()   { M_BIT(4, R.BC.B.l); }
void Z80::bit_4_d()   { M_BIT(4, R.DE.B.h); }
void Z80::bit_4_e()   { M_BIT(4, R.DE.B.l); }
void Z80::bit_4_h()   { M_BIT(4, R.HL.B.h); }
void Z80::bit_4_l()   { M_BIT(4, R.HL.B.l); }

void Z80::bit_5_xhl() { M_BIT(5, M_RD_XHL()); currentTime++; }
void Z80::bit_5_xix() { M_BIT(5, M_RD_XIX()); currentTime++; }
void Z80::bit_5_xiy() { M_BIT(5, M_RD_XIY()); currentTime++; }
void Z80::bit_5_a()   { M_BIT(5, R.AF.B.h); }
void Z80::bit_5_b()   { M_BIT(5, R.BC.B.h); }
void Z80::bit_5_c()   { M_BIT(5, R.BC.B.l); }
void Z80::bit_5_d()   { M_BIT(5, R.DE.B.h); }
void Z80::bit_5_e()   { M_BIT(5, R.DE.B.l); }
void Z80::bit_5_h()   { M_BIT(5, R.HL.B.h); }
void Z80::bit_5_l()   { M_BIT(5, R.HL.B.l); }

void Z80::bit_6_xhl() { M_BIT(6, M_RD_XHL()); currentTime++; }
void Z80::bit_6_xix() { M_BIT(6, M_RD_XIX()); currentTime++; }
void Z80::bit_6_xiy() { M_BIT(6, M_RD_XIY()); currentTime++; }
void Z80::bit_6_a()   { M_BIT(6, R.AF.B.h); }
void Z80::bit_6_b()   { M_BIT(6, R.BC.B.h); }
void Z80::bit_6_c()   { M_BIT(6, R.BC.B.l); }
void Z80::bit_6_d()   { M_BIT(6, R.DE.B.h); }
void Z80::bit_6_e()   { M_BIT(6, R.DE.B.l); }
void Z80::bit_6_h()   { M_BIT(6, R.HL.B.h); }
void Z80::bit_6_l()   { M_BIT(6, R.HL.B.l); }

void Z80::bit_7_xhl() { M_BIT(7, M_RD_XHL()); currentTime++; }
void Z80::bit_7_xix() { M_BIT(7, M_RD_XIX()); currentTime++; }
void Z80::bit_7_xiy() { M_BIT(7, M_RD_XIY()); currentTime++; }
void Z80::bit_7_a()   { M_BIT(7, R.AF.B.h); }
void Z80::bit_7_b()   { M_BIT(7, R.BC.B.h); }
void Z80::bit_7_c()   { M_BIT(7, R.BC.B.l); }
void Z80::bit_7_d()   { M_BIT(7, R.DE.B.h); }
void Z80::bit_7_e()   { M_BIT(7, R.DE.B.l); }
void Z80::bit_7_h()   { M_BIT(7, R.HL.B.h); }
void Z80::bit_7_l()   { M_BIT(7, R.HL.B.l); }

void Z80::call_c()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_C())  M_CALL(adrs); }
void Z80::call_m()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_M())  M_CALL(adrs); }
void Z80::call_nc() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_NC()) M_CALL(adrs); }
void Z80::call_nz() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_NZ()) M_CALL(adrs); }
void Z80::call_p()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_P())  M_CALL(adrs); }
void Z80::call_pe() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_PE()) M_CALL(adrs); }
void Z80::call_po() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_PO()) M_CALL(adrs); }
void Z80::call_z()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_Z())  M_CALL(adrs); }
void Z80::call()    { word adrs = Z80_RDMEM_OPCODE_WORD();             M_CALL(adrs); }

void Z80::ccf() {
	R.AF.B.l = ((R.AF.B.l&(S_FLAG|Z_FLAG|P_FLAG|C_FLAG)|
	          ((R.AF.B.l&C_FLAG)<<4))|
		  XYTable[R.AF.B.h]                       )^1;
}

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
	byte val = Z80_RDMEM(R.HL.w);
	Z80_WRMEM(R.HL.w, val);	// TODO check exact timing
	currentTime += 2;
	byte res = R.AF.B.h - val;
	R.HL.w--; R.BC.w--;
	R.AF.B.l = (R.AF.B.l & C_FLAG)|
	         ((R.AF.B.h^val^res)&H_FLAG)|
	         ZSTable[res]|
	         N_FLAG;
	if (R.AF.B.l & H_FLAG) res -= 1;
	if (res & 0x02) R.AF.B.l |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) R.AF.B.l |= X_FLAG; // bit 3 -> flag 3
	if (R.BC.w) R.AF.B.l |= V_FLAG;
}
void Z80::cpdr() {
	cpd ();
	if (R.BC.w && !(R.AF.B.l&Z_FLAG)) { currentTime+=5; R.PC.w-=2; }
}

void Z80::cpi() {
	byte val = Z80_RDMEM(R.HL.w);
	Z80_WRMEM(R.HL.w, val);	// TODO check exact timing
	currentTime += 2;
	byte res = R.AF.B.h - val;
	R.HL.w++; R.BC.w--;
	R.AF.B.l = (R.AF.B.l & C_FLAG)|
	         ((R.AF.B.h^val^res)&H_FLAG)|
	         ZSTable[res]|
	         N_FLAG;
	if (R.AF.B.l & H_FLAG) res -= 1;
	if (res & 0x02) R.AF.B.l |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) R.AF.B.l |= X_FLAG; // bit 3 -> flag 3
	if (R.BC.w) R.AF.B.l |= V_FLAG;
}
void Z80::cpir() {
	cpi ();
	if (R.BC.w && !(R.AF.B.l&Z_FLAG)) { currentTime+=5; R.PC.w-=2; }
}

void Z80::cpl() { R.AF.B.h^=0xFF; R.AF.B.l|=(H_FLAG|N_FLAG); }

void Z80::daa() {
	int i = R.AF.B.h;
	if (R.AF.B.l&C_FLAG) i|=256;
	if (R.AF.B.l&H_FLAG) i|=512;
	if (R.AF.B.l&N_FLAG) i|=1024;
	R.AF.w = DAATable[i];
}

void Z80::dec_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_DEC(i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::dec_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_DEC(i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::dec_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_DEC(i);
	currentTime++;
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

void Z80::dec_bc() { --R.BC.w; currentTime += 2; }
void Z80::dec_de() { --R.DE.w; currentTime += 2; }
void Z80::dec_hl() { --R.HL.w; currentTime += 2; }
void Z80::dec_ix() { --R.IX.w; currentTime += 2; }
void Z80::dec_iy() { --R.IY.w; currentTime += 2; }
void Z80::dec_sp() { --R.SP.w; currentTime += 2; }

void Z80::di() { R.IFF1 = R.nextIFF1 = R.IFF2 = false; }

void Z80::ei() {
	R.nextIFF1 = true;	// delay one instruction
	R.IFF2 = true;
}

void Z80::djnz() {
	currentTime++; 
	offset e = Z80_RDMEM_OPCODE(); 
	if (--R.BC.B.h) M_JR(e);
}

void Z80::ex_xsp_hl() {
	word temp;
	M_POP(temp);
	M_PUSH(R.HL.w);
	R.HL.w = temp;
	currentTime += 2;
}
void Z80::ex_xsp_ix() {
	word temp;
	M_POP(temp);
	M_PUSH(R.IX.w);
	R.IX.w = temp;
	currentTime += 2;
}
void Z80::ex_xsp_iy()
{
	word temp;
	M_POP(temp);
	M_PUSH(R.IY.w);
	R.IY.w = temp;
	currentTime += 2;
}
void Z80::ex_af_af() {
	int i = R.AF.w;
	R.AF.w = R.AF2.w;
	R.AF2.w = i;
}
void Z80::ex_de_hl() {
	int i = R.DE.w;
	R.DE.w = R.HL.w;
	R.HL.w = i;
}
void Z80::exx() {
	int i;
	i = R.BC.w; R.BC.w = R.BC2.w; R.BC2.w = i;
	i = R.DE.w; R.DE.w = R.DE2.w; R.DE2.w = i;
	i = R.HL.w; R.HL.w = R.HL2.w; R.HL2.w = i;
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
void Z80::in_0_c() { byte i; M_IN(i); } //discard result
void Z80::in_a_byte() { R.AF.B.h = Z80_In(Z80_RDMEM_OPCODE()+(R.AF.B.h<<8)); }

void Z80::inc_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_INC(i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::inc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_INC(i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::inc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_INC(i);
	currentTime++;
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

void Z80::inc_bc() { ++R.BC.w; currentTime += 2; }
void Z80::inc_de() { ++R.DE.w; currentTime += 2; }
void Z80::inc_hl() { ++R.HL.w; currentTime += 2; }
void Z80::inc_ix() { ++R.IX.w; currentTime += 2; }
void Z80::inc_iy() { ++R.IY.w; currentTime += 2; }
void Z80::inc_sp() { ++R.SP.w; currentTime += 2; }

void Z80::ind() {
	currentTime++;
	byte io = Z80_In(R.BC.w);
	R.BC.B.h--;
	Z80_WRMEM(R.HL.w, io);
	R.HL.w--;
	R.AF.B.l = ZSTable[R.BC.B.h];
	if (io&S_FLAG) R.AF.B.l |= N_FLAG;
	if ((((R.BC.B.l+1)&0xff)+io)&0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[R.BC.B.l&3][io&3] ^
	    breg_tmp2[R.BC.B.h] ^
	    (R.BC.B.l>>2) ^
	    (io>>2)                     )&1)
		R.AF.B.l |= P_FLAG;
}
void Z80::indr() {
	ind ();
	if (R.BC.B.h) { currentTime += 5; R.PC.w -= 2; }
}

void Z80::ini() {
	currentTime++;
	byte io = Z80_In(R.BC.w);
	R.BC.B.h--;
	Z80_WRMEM(R.HL.w, io);
	R.HL.w++;
	R.AF.B.l = ZSTable[R.BC.B.h];
	if (io&S_FLAG) R.AF.B.l |= N_FLAG;
	if ((((R.BC.B.l+1)&0xff)+io)&0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[R.BC.B.l&3][io&3] ^
	    breg_tmp2[R.BC.B.h] ^
	    (R.BC.B.l>>2) ^
	    (io>>2)                     )&1)
		R.AF.B.l |= P_FLAG;
}
void Z80::inir() {
	ini ();
	if (R.BC.B.h) { currentTime += 5; R.PC.w -= 2; }
}

void Z80::jp_hl() { R.PC.w = R.HL.w; }
void Z80::jp_ix() { R.PC.w = R.IX.w; }
void Z80::jp_iy() { R.PC.w = R.IY.w; }
void Z80::jp()    { word adrs = Z80_RDMEM_OPCODE_WORD();             M_JP(adrs); }
void Z80::jp_c()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_C())  M_JP(adrs); }
void Z80::jp_m()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_M())  M_JP(adrs); }
void Z80::jp_nc() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_NC()) M_JP(adrs); }
void Z80::jp_nz() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_NZ()) M_JP(adrs); }
void Z80::jp_p()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_P())  M_JP(adrs); }
void Z80::jp_pe() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_PE()) M_JP(adrs); }
void Z80::jp_po() { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_PO()) M_JP(adrs); }
void Z80::jp_z()  { word adrs = Z80_RDMEM_OPCODE_WORD(); if (M_Z())  M_JP(adrs); }

void Z80::jr()    { offset e = Z80_RDMEM_OPCODE();             M_JR(e); }
void Z80::jr_c()  { offset e = Z80_RDMEM_OPCODE(); if (M_C())  M_JR(e); }
void Z80::jr_nc() { offset e = Z80_RDMEM_OPCODE(); if (M_NC()) M_JR(e); }
void Z80::jr_nz() { offset e = Z80_RDMEM_OPCODE(); if (M_NZ()) M_JR(e); }
void Z80::jr_z()  { offset e = Z80_RDMEM_OPCODE(); if (M_Z())  M_JR(e); }

void Z80::ld_xbc_a()   { Z80_WRMEM(R.BC.w, R.AF.B.h); }
void Z80::ld_xde_a()   { Z80_WRMEM(R.DE.w, R.AF.B.h); }
void Z80::ld_xhl_a()   { Z80_WRMEM(R.HL.w, R.AF.B.h); }
void Z80::ld_xhl_b()   { Z80_WRMEM(R.HL.w, R.BC.B.h); }
void Z80::ld_xhl_c()   { Z80_WRMEM(R.HL.w, R.BC.B.l); }
void Z80::ld_xhl_d()   { Z80_WRMEM(R.HL.w, R.DE.B.h); }
void Z80::ld_xhl_e()   { Z80_WRMEM(R.HL.w, R.DE.B.l); }
void Z80::ld_xhl_h()   { Z80_WRMEM(R.HL.w, R.HL.B.h); }
void Z80::ld_xhl_l()   { Z80_WRMEM(R.HL.w, R.HL.B.l); }
void Z80::ld_xhl_byte(){ Z80_WRMEM(R.HL.w, Z80_RDMEM_OPCODE()); }
void Z80::ld_xix_a()   { M_WR_XIX(R.AF.B.h); }
void Z80::ld_xix_b()   { M_WR_XIX(R.BC.B.h); }
void Z80::ld_xix_c()   { M_WR_XIX(R.BC.B.l); }
void Z80::ld_xix_d()   { M_WR_XIX(R.DE.B.h); }
void Z80::ld_xix_e()   { M_WR_XIX(R.DE.B.l); }
void Z80::ld_xix_h()   { M_WR_XIX(R.HL.B.h); }
void Z80::ld_xix_l()   { M_WR_XIX(R.HL.B.l); }
void Z80::ld_xix_byte(){
	int j = (R.IX.w+(offset)Z80_RDMEM_OPCODE())&0xFFFF;
	byte i = Z80_RDMEM_OPCODE();
	currentTime += 2;	// parallelism
	Z80_WRMEM(j, i);
}
void Z80::ld_xiy_a()   { M_WR_XIY(R.AF.B.h); }
void Z80::ld_xiy_b()   { M_WR_XIY(R.BC.B.h); }
void Z80::ld_xiy_c()   { M_WR_XIY(R.BC.B.l); }
void Z80::ld_xiy_d()   { M_WR_XIY(R.DE.B.h); }
void Z80::ld_xiy_e()   { M_WR_XIY(R.DE.B.l); }
void Z80::ld_xiy_h()   { M_WR_XIY(R.HL.B.h); }
void Z80::ld_xiy_l()   { M_WR_XIY(R.HL.B.l); }
void Z80::ld_xiy_byte(){
	int j = (R.IY.w+(offset)Z80_RDMEM_OPCODE())&0xFFFF;
	byte i = Z80_RDMEM_OPCODE();
	currentTime += 2;	// parallelism
	Z80_WRMEM(j, i);
}
void Z80::ld_xbyte_a() { Z80_WRMEM(Z80_RDMEM_OPCODE_WORD(), R.AF.B.h); }
void Z80::ld_xword_bc(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.BC.w); }
void Z80::ld_xword_de(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.DE.w); }
void Z80::ld_xword_hl(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.HL.w); }
void Z80::ld_xword_ix(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.IX.w); }
void Z80::ld_xword_iy(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.IY.w); }
void Z80::ld_xword_sp(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), R.SP.w); }
void Z80::ld_a_xbc()   { R.AF.B.h = Z80_RDMEM(R.BC.w); }
void Z80::ld_a_xde()   { R.AF.B.h = Z80_RDMEM(R.DE.w); }
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
void Z80::ld_bc_xword(){ R.BC.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_bc_word() { R.BC.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_de_xword(){ R.DE.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_de_word() { R.DE.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_hl_xword(){ R.HL.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_hl_word() { R.HL.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_ix_xword(){ R.IX.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_ix_word() { R.IX.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_iy_xword(){ R.IY.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_iy_word() { R.IY.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_xword(){ R.SP.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_sp_word() { R.SP.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_hl()   { R.SP.w = R.HL.w; }
void Z80::ld_sp_ix()   { R.SP.w = R.IX.w; }
void Z80::ld_sp_iy()   { R.SP.w = R.IY.w; }
void Z80::ld_a_i() {
	currentTime++;
	R.AF.B.h = R.I;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSXYTable[R.I]|((R.IFF2 ? V_FLAG : 0));
}
void Z80::ld_i_a()     { currentTime++; R.I = R.AF.B.h; }
void Z80::ld_a_r() {
	currentTime++;
	R.AF.B.h=(R.R&127)|(R.R2&128);
	R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSXYTable[R.AF.B.h]|((R.IFF2 ? V_FLAG : 0));
}
void Z80::ld_r_a()     { currentTime++; R.R = R.R2 = R.AF.B.h; }

void Z80::ldd() {
	byte io = Z80_RDMEM(R.HL.w);
	Z80_WRMEM(R.DE.w, io);        
	currentTime += 2;
	R.AF.B.l &= S_FLAG | Z_FLAG | C_FLAG;
	if ((R.AF.B.h+io)&0x02) R.AF.B.l |= Y_FLAG;	// bit 1 -> flag 5
	if ((R.AF.B.h+io)&0x08) R.AF.B.l |= X_FLAG;	// bit 3 -> flag 3
	R.HL.w--; R.DE.w--; R.BC.w--;
	if (R.BC.w) R.AF.B.l |= V_FLAG;
}
void Z80::lddr() {
	ldd ();
	if (R.BC.w) { currentTime += 5; R.PC.w -= 2; }
}
void Z80::ldi() {
	byte io = Z80_RDMEM(R.HL.w);
	Z80_WRMEM(R.DE.w, io);        
	currentTime += 2;
	R.AF.B.l &= S_FLAG | Z_FLAG | C_FLAG;
	if ((R.AF.B.h+io)&0x02) R.AF.B.l |= Y_FLAG;	// bit 1 -> flag 5
	if ((R.AF.B.h+io)&0x08) R.AF.B.l |= X_FLAG;	// bit 3 -> flag 3
	R.HL.w++; R.DE.w++; R.BC.w--;
	if (R.BC.w) R.AF.B.l |= V_FLAG;
}
void Z80::ldir() {
	ldi ();
	if (R.BC.w) { currentTime += 5; R.PC.w -= 2; }
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
void Z80::or_a()   { R.AF.B.l = ZSPXYTable[R.AF.B.h]; }
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
	currentTime++;
	byte io = Z80_RDMEM(R.HL.w--);
	R.BC.B.h--;
	Z80_Out(R.BC.w, io);
	R.AF.B.l = ZSTable[R.BC.B.h];
	if (io&S_FLAG) R.AF.B.l |= N_FLAG;
	if ((((R.BC.B.l+1)&0xff)+io)&0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[R.BC.B.l&3][io&3] ^
	    breg_tmp2[R.BC.B.h] ^
	    (R.BC.B.l>>2) ^
	    (io>>2))                    & 1 )
		R.AF.B.l |= P_FLAG;
}
void Z80::otdr() {
	outd ();
	if (R.BC.B.h) { currentTime += 5; R.PC.w -= 2; }
}
void Z80::outi() {
	currentTime++;
	byte io = Z80_RDMEM(R.HL.w++);
	R.BC.B.h--;
	Z80_Out(R.BC.w, io);
	R.AF.B.l = ZSTable[R.BC.B.h];
	if (io&S_FLAG) R.AF.B.l |= N_FLAG;
	if ((((R.BC.B.l+1)&0xff)+io)&0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[R.BC.B.l&3][io&3] ^
	    breg_tmp2[R.BC.B.h] ^
	    (R.BC.B.l>>2) ^
	    (io>>2))                    & 1 )
		R.AF.B.l |= P_FLAG;
}
void Z80::otir() {
	outi ();
	if (R.BC.B.h) { currentTime += 5; R.PC.w -= 2; }
}

void Z80::out_c_a()   { Z80_Out(R.BC.w, R.AF.B.h); }
void Z80::out_c_b()   { Z80_Out(R.BC.w, R.BC.B.h); }
void Z80::out_c_c()   { Z80_Out(R.BC.w, R.BC.B.l); }
void Z80::out_c_d()   { Z80_Out(R.BC.w, R.DE.B.h); }
void Z80::out_c_e()   { Z80_Out(R.BC.w, R.DE.B.l); }
void Z80::out_c_h()   { Z80_Out(R.BC.w, R.HL.B.h); }
void Z80::out_c_l()   { Z80_Out(R.BC.w, R.HL.B.l); }
void Z80::out_c_0()   { Z80_Out(R.BC.w, 0); }
void Z80::out_byte_a(){ Z80_Out(Z80_RDMEM_OPCODE()+(R.AF.B.h<<8), R.AF.B.h); }

void Z80::pop_af() { M_POP(R.AF.w); }
void Z80::pop_bc() { M_POP(R.BC.w); }
void Z80::pop_de() { M_POP(R.DE.w); }
void Z80::pop_hl() { M_POP(R.HL.w); }
void Z80::pop_ix() { M_POP(R.IX.w); }
void Z80::pop_iy() { M_POP(R.IY.w); }

void Z80::push_af() { M_PUSH(R.AF.w); }
void Z80::push_bc() { M_PUSH(R.BC.w); }
void Z80::push_de() { M_PUSH(R.DE.w); }
void Z80::push_hl() { M_PUSH(R.HL.w); }
void Z80::push_ix() { M_PUSH(R.IX.w); }
void Z80::push_iy() { M_PUSH(R.IY.w); }

void Z80::res_0_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(0, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(1, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(2, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(3, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(4, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(5, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(6, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	currentTime++;
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RES(7, i);
	currentTime++;
	Z80_WRMEM(R.HL.w, i);
}
void Z80::res_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	currentTime++;
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_a() { M_RES(7, R.AF.B.h); };
void Z80::res_7_b() { M_RES(7, R.BC.B.h); };
void Z80::res_7_c() { M_RES(7, R.BC.B.l); };
void Z80::res_7_d() { M_RES(7, R.DE.B.h); };
void Z80::res_7_e() { M_RES(7, R.DE.B.l); };
void Z80::res_7_h() { M_RES(7, R.HL.B.h); };
void Z80::res_7_l() { M_RES(7, R.HL.B.l); };

void Z80::ret()    {                            M_RET(); }
void Z80::ret_c()  { currentTime++; if (M_C())  M_RET(); }
void Z80::ret_m()  { currentTime++; if (M_M())  M_RET(); }
void Z80::ret_nc() { currentTime++; if (M_NC()) M_RET(); }
void Z80::ret_nz() { currentTime++; if (M_NZ()) M_RET(); }
void Z80::ret_p()  { currentTime++; if (M_P())  M_RET(); }
void Z80::ret_pe() { currentTime++; if (M_PE()) M_RET(); }
void Z80::ret_po() { currentTime++; if (M_PO()) M_RET(); }
void Z80::ret_z()  { currentTime++; if (M_Z())  M_RET(); }

void Z80::reti() { R.IFF1 = R.nextIFF1 = R.IFF2; interface->reti(); M_RET(); }	// same as retn!!
void Z80::retn() { R.IFF1 = R.nextIFF1 = R.IFF2; interface->retn(); M_RET(); }

void Z80::rl_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_RL(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::rl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RLC(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::rlc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}

void Z80::rlc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	currentTime += 4;
	Z80_WRMEM(R.HL.w, (i<<4)|(R.AF.B.h&0x0F));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i>>4);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPXYTable[R.AF.B.h];
}

void Z80::rr_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_RR(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::rr_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_RRC(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::rrc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	currentTime += 4; 
	Z80_WRMEM(R.HL.w, (i>>4)|(R.AF.B.h<<4));
	R.AF.B.h = (R.AF.B.h&0xF0)|(i&0x0F);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPXYTable[R.AF.B.h];
}

void Z80::rst_00() { M_CALL(0x00); }
void Z80::rst_08() { M_CALL(0x08); }
void Z80::rst_10() { M_CALL(0x10); }
void Z80::rst_18() { M_CALL(0x18); }
void Z80::rst_20() { M_CALL(0x20); }
void Z80::rst_28() { M_CALL(0x28); }
void Z80::rst_30() { M_CALL(0x30); }
void Z80::rst_38() { M_CALL(0x38); }

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

void Z80::sbc_hl_bc() { M_SBCW(R.BC.w); }
void Z80::sbc_hl_de() { M_SBCW(R.DE.w); }
void Z80::sbc_hl_hl() { M_SBCW(R.HL.w); }
void Z80::sbc_hl_sp() { M_SBCW(R.SP.w); }

void Z80::scf() { 
	R.AF.B.l = (R.AF.B.l&(S_FLAG|Z_FLAG|P_FLAG))|
	         C_FLAG|
	         XYTable[R.AF.B.h];
}

void Z80::set_0_xhl() {
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(0, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(1, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(2, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(3, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(4, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(5, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(6, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SET(7, i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::set_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SLA(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::sla_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	currentTime++; 
	M_SLL(i);
	Z80_WRMEM(R.HL.w, i);
}
void Z80::sll_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SRA(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::sra_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	currentTime++; 
	R.HL.B.l = i;
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
	byte i = Z80_RDMEM(R.HL.w);
	M_SRL(i);
	currentTime++; 
	Z80_WRMEM(R.HL.w, i);
}
void Z80::srl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	currentTime++; 
	R.HL.B.l = i;
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
void Z80::sub_a()   { R.AF.w = Z_FLAG|N_FLAG; }
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
void Z80::xor_a()   { R.AF.w = Z_FLAG|V_FLAG;}
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


void Z80::patch() { interface->patch(); }



void Z80::dd_cb() {
	byte opcode = Z80_RDOP_ARG((R.PC.w+1)&0xFFFF);
	currentTime++;	// TODO correct timing
	(this->*opcode_dd_cb[opcode])();
	R.PC.w++;
}
void Z80::fd_cb() {
	byte opcode = Z80_RDOP_ARG((R.PC.w+1)&0xFFFF);
	currentTime++;	// TODO correct timing
	(this->*opcode_fd_cb[opcode])();
	R.PC.w++;
}
void Z80::cb() {
	M1Cycle();
	byte opcode = Z80_RDOP(R.PC.w++);
	(this->*opcode_cb[opcode])();
}
void Z80::ed() {
	M1Cycle();
	byte opcode = Z80_RDOP(R.PC.w++);
	(this->*opcode_ed[opcode])();
}
void Z80::dd() {
	M1Cycle();
	byte opcode = Z80_RDOP(R.PC.w++);
	(this->*opcode_dd[opcode])();
}
void Z80::dd2() {
	byte opcode = Z80_RDOP(R.PC.w++);
	currentTime++;
	(this->*opcode_dd[opcode])();
}
void Z80::fd() {
	M1Cycle();
	byte opcode = Z80_RDOP(R.PC.w++);
	(this->*opcode_fd[opcode])();
}
void Z80::fd2() {
	byte opcode = Z80_RDOP(R.PC.w++);
	currentTime++;
	(this->*opcode_fd[opcode])();
}

