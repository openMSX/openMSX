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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include "Z80.hh"
#include "Z80Tables.hh"

#ifdef Z80DEBUG
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
}

/****************************************************************************/
/* Reset the CPU emulation core                                             */
/* Set registers to their initial values                                    */
/* and make the Z80 the starting CPU                                        */
/****************************************************************************/
void Z80::reset()
{
	// AF and SP are 0xffff
	// PC, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	AF.w = 0xffff;
	BC.w = 0xffff;
	DE.w = 0xffff;
	HL.w = 0xffff;
	IX.w = 0xffff;
	IY.w = 0xffff;
	PC.w = 0x0000;
	SP.w = 0xffff;
	
	AF2.w = 0xffff;
	BC2.w = 0xffff;
	DE2.w = 0xffff;
	HL2.w = 0xffff;

	nextIFF1 = false;
	IFF1 = false;
	IFF2 = false;
	HALT = false;
	
	IM = 0;
	I = 0xff;
	R = 0x7f;
	R2 = 0;
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/
int Z80::Z80_SingleInstruction() 
{
	byte opcode;
	
	if (interface->NMIStatus()) { 
		// TODO what prevents ack an NMI more than once????
		// NMI occured
		HALT = false; 
		IFF1 = nextIFF1 = false;
		M_PUSH (PC.w);
		PC.w=0x0066;
		return 10;	//TODO this value is wrong
				//TODO ++R
	} 
	if (IFF1 && interface->IRQStatus()) {
		// normal interrupt
		HALT = false; 
		IFF1 = nextIFF1 = false;
		switch (IM) {
		case 2:
			// Interrupt mode 2  Call [I:databyte]
			M_PUSH (PC.w);
			PC.w=Z80_RDMEM_WORD((interface->dataBus())|(I<<8));
			return 10;	//TODO this value is wrong
					//TODO ++R
		case 1:
			// Interrupt mode 1  RST 38h
			opcode = 0xff;
			break;
		case 0:
			// Interrupt mode 0
			// TODO current implementation only works for 1-byte instructions
			//      ok for MSX 
			opcode = interface->dataBus();
			break;
		default:
			assert(false);
			opcode = 0;	// prevent warning
		}
	} else if (HALT) {
		opcode = 0;	// nop
	} else {
		opcode = Z80_RDOP(PC.w++);
	}
	IFF1 = nextIFF1;
	#ifdef Z80DEBUG
		word start_pc = PC.w-1;
	#endif
	++R;
	ICount = cycles_main[opcode];
	(this->*opcode_main[opcode])();	// ICount can be raised extra
	#ifdef Z80DEBUG
		printf("%04x : instruction ", start_pc);
		Z80_Dasm(&debugmemory[start_pc], to_print_string, start_pc );
		printf("%s\n", to_print_string );
		printf("      A=%02x F=%02x \n", AF.B.h, AF.B.l);
		printf("      BC=%04x DE=%04x HL=%04x \n", BC.w, DE.w, HL.w);
		printf("  took %d Tstates\n", ICount);
	#endif
	//assert(ICount>0);
	return ICount;
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80::Z80_SetWaitStates (int n)
{
	int diff = n - waitStates;
	waitStates = n;
	for (int i=0; i<256; ++i) {
		cycles_main[i] += diff;
		cycles_cb[i]   += diff;
		cycles_ed[i]   += diff;
		cycles_xx[i]   += diff;
		//cycles_xx_cb[i]+= diff;	//TODO check this not needed
	}
}
int Z80::waitStates = 0;


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
#ifdef Z80DEBUG
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


inline void Z80::M_SKIP_CALL() { PC.w += 2; }
inline void Z80::M_SKIP_JP()   { PC.w += 2; }
inline void Z80::M_SKIP_JR()   { PC.w += 1; }
inline void Z80::M_SKIP_RET()  { }

inline bool Z80::M_C()  { return AF.B.l&C_FLAG; }
inline bool Z80::M_NC() { return !M_C(); }
inline bool Z80::M_Z()  { return AF.B.l&Z_FLAG; }
inline bool Z80::M_NZ() { return !M_Z(); }
inline bool Z80::M_M()  { return AF.B.l&S_FLAG; }
inline bool Z80::M_P()  { return !M_M(); }
inline bool Z80::M_PE() { return AF.B.l&V_FLAG; }
inline bool Z80::M_PO() { return !M_PE(); }

/* Get next opcode argument and increment program counter */
inline byte Z80::Z80_RDMEM_OPCODE () {
	return Z80_RDOP_ARG(PC.w++);
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

inline word Z80::M_XIX() { return (IX.w+(offset)Z80_RDMEM_OPCODE())&0xFFFF; }
inline word Z80::M_XIY() { return (IY.w+(offset)Z80_RDMEM_OPCODE())&0xFFFF; }
inline byte Z80::M_RD_XHL() { return Z80_RDMEM(HL.w); }
inline byte Z80::M_RD_XIX() { return Z80_RDMEM(M_XIX()); }
inline byte Z80::M_RD_XIY() { return Z80_RDMEM(M_XIY()); }
inline void Z80::M_WR_XIX(byte a) { Z80_WRMEM(M_XIX(), a); }
inline void Z80::M_WR_XIY(byte a) { Z80_WRMEM(M_XIY(), a); }


inline void Z80::M_POP(word &Rg) {
	Rg = Z80_RDSTACK(SP.w)+(Z80_RDSTACK((SP.w+1)&65535)<<8);
	SP.w += 2;
}

inline void Z80::M_PUSH(word Rg) {
	SP.w -= 2;
	Z80_WRSTACK(SP.w, Rg);
	Z80_WRSTACK((SP.w+1)&65535, Rg>>8);
}

inline void Z80::M_CALL() {
	word q = Z80_RDMEM_OPCODE_WORD();
	M_PUSH(PC.w);
	PC.w = q;
	ICount += 7; // extraTab
}

inline void Z80::M_JR() {
	PC.w += ((offset)Z80_RDOP_ARG(PC.w))+1;
	ICount += 5; // extraTab
}

inline void Z80::M_RET() {
	M_POP(PC.w);
	ICount += 6; // extraTab
}

inline void Z80::M_JP() {
        PC.w = Z80_RDOP_ARG(PC.w)+((Z80_RDOP_ARG((PC.w+1)&65535))<<8);
}

inline void Z80::M_RST(word Addr) {
	M_PUSH(PC.w);
	PC.w = Addr;
}

inline void Z80::M_SET(int Bit, byte &Reg) { 
	Reg |= 1<<Bit;
}
inline void Z80::M_RES(int Bit, byte &Reg) {
	Reg &= ~(1<<Bit);
}
inline void Z80::M_BIT(int Bit, byte Reg) {
	// TODO  not for bit n,(hl)
	//               bit n,(ix+d)      
	AF.B.l = (AF.B.l&C_FLAG) | ZSPXYTable[Reg & (1<<Bit)];
}

inline void Z80::M_AND(byte Reg) {
	AF.B.h &= Reg;
	AF.B.l = ZSPXYTable[AF.B.h]|H_FLAG;
}
inline void Z80::M_OR(byte Reg) {
	AF.B.h |= Reg;
	AF.B.l = ZSPXYTable[AF.B.h];
}
inline void Z80::M_XOR(byte Reg) {
	AF.B.h ^= Reg;
	AF.B.l = ZSPXYTable[AF.B.h];
}

inline void Z80::M_IN(byte &Reg) {
	Reg = Z80_In(BC.w);
	AF.B.l = (AF.B.l&C_FLAG)|ZSPXYTable[Reg];
}

inline void Z80::M_RLCA() {
	AF.B.h = (AF.B.h<<1)|((AF.B.h&0x80)>>7);
	AF.B.l = (AF.B.l&0xC4)|(AF.B.h&C_FLAG);
	AF.B.l |= XYTable[AF.B.h];
}
inline void Z80::M_RRCA() {
	AF.B.l = (AF.B.l&0xC4)|(AF.B.h&C_FLAG);
	AF.B.h = (AF.B.h>>1)|(AF.B.h<<7);
	AF.B.l |= XYTable[AF.B.h];
}
inline void Z80::M_RLA() {
	byte i = AF.B.l&C_FLAG;
	AF.B.l = (AF.B.l&0xC4)|((AF.B.h&0x80)>>7);
	AF.B.h = (AF.B.h<<1)|i;
	AF.B.l |= XYTable[AF.B.h];
}
inline void Z80::M_RRA() {
	byte i = AF.B.l&C_FLAG;
	AF.B.l = (AF.B.l&0xC4)|(AF.B.h&0x01);
	AF.B.h = (AF.B.h>>1)|(i<<7);
	AF.B.l |= XYTable[AF.B.h];
}

inline void Z80::M_RLC(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|q;
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_RRC(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(q<<7);
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_RL(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|(AF.B.l&1);
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_RR(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(AF.B.l<<7);
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_SLL(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|1;
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_SLA(byte &Reg) {
	byte q = Reg>>7;
	Reg <<= 1;
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_SRL(byte &Reg) {
	byte q = Reg&1;
	Reg >>= 1;
	AF.B.l = ZSPXYTable[Reg]|q;
}
inline void Z80::M_SRA(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(Reg&0x80);
	AF.B.l = ZSPXYTable[Reg]|q;
}

inline void Z80::M_INC(byte &Reg) {
	++Reg;
	AF.B.l = (AF.B.l&C_FLAG)|
	         ((Reg==0x80) ? V_FLAG : 0)|
	         (((Reg&0x0f)==0x00) ? H_FLAG : 0)|
	         ZSXYTable[Reg];
}
inline void Z80::M_DEC(byte &Reg) {
	Reg--;
	AF.B.l = (AF.B.l&C_FLAG)|
	         ((Reg==0x7f) ? V_FLAG : 0)|
	         (((Reg&0x0f)==0x0f) ? H_FLAG : 0)|
	         ZSXYTable[Reg]|
	         N_FLAG;
}

inline void Z80::M_ADD(byte Reg) {
	int q = AF.B.h + Reg;
	AF.B.l = ZSXYTable[q&255]|
	         ((q&256)>>8)|
	         ((AF.B.h^q^Reg)&H_FLAG)|
	         (((Reg^AF.B.h^0x80)&(Reg^q)&0x80)>>5);
	AF.B.h = q;
}
inline void Z80::M_ADC(byte Reg) {
	int q = AF.B.h + Reg + (AF.B.l&C_FLAG);
	AF.B.l = ZSXYTable[q&255]|
	         ((q&256)>>8)|
	         ((AF.B.h^q^Reg)&H_FLAG)|
	         (((Reg^AF.B.h^0x80)&(Reg^q)&0x80)>>5);
	AF.B.h = q;
}
inline void Z80::M_SUB(byte Reg) {
	int q = AF.B.h - Reg;
	AF.B.l = ZSXYTable[q&255]|
	         ((q&256)>>8)|
	         N_FLAG|
	         ((AF.B.h^q^Reg)&H_FLAG)|
	         (((Reg^AF.B.h)&(Reg^q)&0x80)>>5);
	AF.B.h = q;
}
inline void Z80::M_SBC(byte Reg) {
	int q = AF.B.h - Reg - (AF.B.l&C_FLAG);
	AF.B.l = ZSXYTable[q&255]|
	         ((q&256)>>8)|
	         N_FLAG|
	         ((AF.B.h^q^Reg)&H_FLAG)|
	         (((Reg^AF.B.h)&(Reg^q)&0x80)>>5);
	AF.B.h = q;
}
inline void Z80::M_CP(byte Reg) {
	int q = AF.B.h - Reg;
	AF.B.l = ZSTable[q&255]|
	         XYTable[Reg]|	// XY from operand, not from result
	         ((q&256)>>8)|
	         N_FLAG|
	         ((AF.B.h^q^Reg)&H_FLAG)|
	         (((Reg^AF.B.h)&(Reg^q)&0x80)>>5);
}

inline void Z80::M_ADDW(word &Reg1, word Reg2) {
	int q = Reg1 + Reg2;
	AF.B.l = (AF.B.l&(S_FLAG|Z_FLAG|V_FLAG))|
	         (((Reg1^q^Reg2)&0x1000)>>8)|
	         ((q>>16)&C_FLAG)|
	         XYTable[(q>>8)&255];
	Reg1 = q;
}
inline void Z80::M_ADCW(word Reg) {
	int q = HL.w+Reg+(AF.B.l&C_FLAG);
	AF.B.l = (((HL.w^q^Reg)&0x1000)>>8)|
	         ((q>>16)&C_FLAG)|
	         ((q&0x8000)>>8)|
	         ((q&0xffff)?0:Z_FLAG)|
	         (((Reg^HL.w^0x8000)&(Reg^q)&0x8000)>>13)|
	         XYTable[(q>>8)&255];
	HL.w = q;
}
inline void Z80::M_SBCW(word Reg) {
	int q = HL.w-Reg-(AF.B.l&C_FLAG);
	AF.B.l = (((HL.w^q^Reg)&0x1000)>>8)|
	           ((q>>16)&C_FLAG)|
	           ((q&0x8000)>>8)|
	           ((q&0xffff)?0:Z_FLAG)|
	           (((Reg^HL.w)&(Reg^q)&0x8000)>>13)|
		   XYTable[(q>>8)&255]|
	           N_FLAG;
	HL.w = q;
}


void Z80::adc_a_xhl() { M_ADC(M_RD_XHL()); }
void Z80::adc_a_xix() { M_ADC(M_RD_XIX()); }
void Z80::adc_a_xiy() { M_ADC(M_RD_XIY()); }
void Z80::adc_a_a()   { M_ADC(AF.B.h); }
void Z80::adc_a_b()   { M_ADC(BC.B.h); }
void Z80::adc_a_c()   { M_ADC(BC.B.l); }
void Z80::adc_a_d()   { M_ADC(DE.B.h); }
void Z80::adc_a_e()   { M_ADC(DE.B.l); }
void Z80::adc_a_h()   { M_ADC(HL.B.h); }
void Z80::adc_a_l()   { M_ADC(HL.B.l); }
void Z80::adc_a_ixl() { M_ADC(IX.B.l); }
void Z80::adc_a_ixh() { M_ADC(IX.B.h); }
void Z80::adc_a_iyl() { M_ADC(IY.B.l); }
void Z80::adc_a_iyh() { M_ADC(IY.B.h); }
void Z80::adc_a_byte(){ M_ADC(Z80_RDMEM_OPCODE()); }

void Z80::adc_hl_bc() { M_ADCW(BC.w); }
void Z80::adc_hl_de() { M_ADCW(DE.w); }
void Z80::adc_hl_hl() { M_ADCW(HL.w); }
void Z80::adc_hl_sp() { M_ADCW(SP.w); }

void Z80::add_a_xhl() { M_ADD(M_RD_XHL()); }
void Z80::add_a_xix() { M_ADD(M_RD_XIX()); }
void Z80::add_a_xiy() { M_ADD(M_RD_XIY()); }
void Z80::add_a_a()   { M_ADD(AF.B.h); }
void Z80::add_a_b()   { M_ADD(BC.B.h); }
void Z80::add_a_c()   { M_ADD(BC.B.l); }
void Z80::add_a_d()   { M_ADD(DE.B.h); }
void Z80::add_a_e()   { M_ADD(DE.B.l); }
void Z80::add_a_h()   { M_ADD(HL.B.h); }
void Z80::add_a_l()   { M_ADD(HL.B.l); }
void Z80::add_a_ixl() { M_ADD(IX.B.l); }
void Z80::add_a_ixh() { M_ADD(IX.B.h); }
void Z80::add_a_iyl() { M_ADD(IY.B.l); }
void Z80::add_a_iyh() { M_ADD(IY.B.h); }
void Z80::add_a_byte(){ M_ADD(Z80_RDMEM_OPCODE()); }

void Z80::add_hl_bc() { M_ADDW(HL.w, BC.w); }
void Z80::add_hl_de() { M_ADDW(HL.w, DE.w); }
void Z80::add_hl_hl() { M_ADDW(HL.w, HL.w); }
void Z80::add_hl_sp() { M_ADDW(HL.w, SP.w); }
void Z80::add_ix_bc() { M_ADDW(IX.w, BC.w); }
void Z80::add_ix_de() { M_ADDW(IX.w, DE.w); }
void Z80::add_ix_ix() { M_ADDW(IX.w, IX.w); }
void Z80::add_ix_sp() { M_ADDW(IX.w, SP.w); }
void Z80::add_iy_bc() { M_ADDW(IY.w, BC.w); }
void Z80::add_iy_de() { M_ADDW(IY.w, DE.w); }
void Z80::add_iy_iy() { M_ADDW(IY.w, IY.w); }
void Z80::add_iy_sp() { M_ADDW(IY.w, SP.w); }

void Z80::and_xhl() { M_AND(M_RD_XHL()); }
void Z80::and_xix() { M_AND(M_RD_XIX()); }
void Z80::and_xiy() { M_AND(M_RD_XIY()); }
void Z80::and_a()   { AF.B.l = ZSPXYTable[AF.B.h]|H_FLAG; }
void Z80::and_b()   { M_AND(BC.B.h); }
void Z80::and_c()   { M_AND(BC.B.l); }
void Z80::and_d()   { M_AND(DE.B.h); }
void Z80::and_e()   { M_AND(DE.B.l); }
void Z80::and_h()   { M_AND(HL.B.h); }
void Z80::and_l()   { M_AND(HL.B.l); }
void Z80::and_ixh() { M_AND(IX.B.h); }
void Z80::and_ixl() { M_AND(IX.B.l); }
void Z80::and_iyh() { M_AND(IY.B.h); }
void Z80::and_iyl() { M_AND(IY.B.l); }
void Z80::and_byte(){ M_AND(Z80_RDMEM_OPCODE()); }

void Z80::bit_0_xhl() { M_BIT(0, M_RD_XHL()); }
void Z80::bit_0_xix() { M_BIT(0, M_RD_XIX()); }
void Z80::bit_0_xiy() { M_BIT(0, M_RD_XIY()); }
void Z80::bit_0_a()   { M_BIT(0, AF.B.h); }
void Z80::bit_0_b()   { M_BIT(0, BC.B.h); }
void Z80::bit_0_c()   { M_BIT(0, BC.B.l); }
void Z80::bit_0_d()   { M_BIT(0, DE.B.h); }
void Z80::bit_0_e()   { M_BIT(0, DE.B.l); }
void Z80::bit_0_h()   { M_BIT(0, HL.B.h); }
void Z80::bit_0_l()   { M_BIT(0, HL.B.l); }

void Z80::bit_1_xhl() { M_BIT(1, M_RD_XHL()); }
void Z80::bit_1_xix() { M_BIT(1, M_RD_XIX()); }
void Z80::bit_1_xiy() { M_BIT(1, M_RD_XIY()); }
void Z80::bit_1_a()   { M_BIT(1, AF.B.h); }
void Z80::bit_1_b()   { M_BIT(1, BC.B.h); }
void Z80::bit_1_c()   { M_BIT(1, BC.B.l); }
void Z80::bit_1_d()   { M_BIT(1, DE.B.h); }
void Z80::bit_1_e()   { M_BIT(1, DE.B.l); }
void Z80::bit_1_h()   { M_BIT(1, HL.B.h); }
void Z80::bit_1_l()   { M_BIT(1, HL.B.l); }

void Z80::bit_2_xhl() { M_BIT(2, M_RD_XHL()); }
void Z80::bit_2_xix() { M_BIT(2, M_RD_XIX()); }
void Z80::bit_2_xiy() { M_BIT(2, M_RD_XIY()); }
void Z80::bit_2_a()   { M_BIT(2, AF.B.h); }
void Z80::bit_2_b()   { M_BIT(2, BC.B.h); }
void Z80::bit_2_c()   { M_BIT(2, BC.B.l); }
void Z80::bit_2_d()   { M_BIT(2, DE.B.h); }
void Z80::bit_2_e()   { M_BIT(2, DE.B.l); }
void Z80::bit_2_h()   { M_BIT(2, HL.B.h); }
void Z80::bit_2_l()   { M_BIT(2, HL.B.l); }

void Z80::bit_3_xhl() { M_BIT(3, M_RD_XHL()); }
void Z80::bit_3_xix() { M_BIT(3, M_RD_XIX()); }
void Z80::bit_3_xiy() { M_BIT(3, M_RD_XIY()); }
void Z80::bit_3_a()   { M_BIT(3, AF.B.h); }
void Z80::bit_3_b()   { M_BIT(3, BC.B.h); }
void Z80::bit_3_c()   { M_BIT(3, BC.B.l); }
void Z80::bit_3_d()   { M_BIT(3, DE.B.h); }
void Z80::bit_3_e()   { M_BIT(3, DE.B.l); }
void Z80::bit_3_h()   { M_BIT(3, HL.B.h); }
void Z80::bit_3_l()   { M_BIT(3, HL.B.l); }

void Z80::bit_4_xhl() { M_BIT(4, M_RD_XHL()); }
void Z80::bit_4_xix() { M_BIT(4, M_RD_XIX()); }
void Z80::bit_4_xiy() { M_BIT(4, M_RD_XIY()); }
void Z80::bit_4_a()   { M_BIT(4, AF.B.h); }
void Z80::bit_4_b()   { M_BIT(4, BC.B.h); }
void Z80::bit_4_c()   { M_BIT(4, BC.B.l); }
void Z80::bit_4_d()   { M_BIT(4, DE.B.h); }
void Z80::bit_4_e()   { M_BIT(4, DE.B.l); }
void Z80::bit_4_h()   { M_BIT(4, HL.B.h); }
void Z80::bit_4_l()   { M_BIT(4, HL.B.l); }

void Z80::bit_5_xhl() { M_BIT(5, M_RD_XHL()); }
void Z80::bit_5_xix() { M_BIT(5, M_RD_XIX()); }
void Z80::bit_5_xiy() { M_BIT(5, M_RD_XIY()); }
void Z80::bit_5_a()   { M_BIT(5, AF.B.h); }
void Z80::bit_5_b()   { M_BIT(5, BC.B.h); }
void Z80::bit_5_c()   { M_BIT(5, BC.B.l); }
void Z80::bit_5_d()   { M_BIT(5, DE.B.h); }
void Z80::bit_5_e()   { M_BIT(5, DE.B.l); }
void Z80::bit_5_h()   { M_BIT(5, HL.B.h); }
void Z80::bit_5_l()   { M_BIT(5, HL.B.l); }

void Z80::bit_6_xhl() { M_BIT(6, M_RD_XHL()); }
void Z80::bit_6_xix() { M_BIT(6, M_RD_XIX()); }
void Z80::bit_6_xiy() { M_BIT(6, M_RD_XIY()); }
void Z80::bit_6_a()   { M_BIT(6, AF.B.h); }
void Z80::bit_6_b()   { M_BIT(6, BC.B.h); }
void Z80::bit_6_c()   { M_BIT(6, BC.B.l); }
void Z80::bit_6_d()   { M_BIT(6, DE.B.h); }
void Z80::bit_6_e()   { M_BIT(6, DE.B.l); }
void Z80::bit_6_h()   { M_BIT(6, HL.B.h); }
void Z80::bit_6_l()   { M_BIT(6, HL.B.l); }

void Z80::bit_7_xhl() { M_BIT(7, M_RD_XHL()); }
void Z80::bit_7_xix() { M_BIT(7, M_RD_XIX()); }
void Z80::bit_7_xiy() { M_BIT(7, M_RD_XIY()); }
void Z80::bit_7_a()   { M_BIT(7, AF.B.h); }
void Z80::bit_7_b()   { M_BIT(7, BC.B.h); }
void Z80::bit_7_c()   { M_BIT(7, BC.B.l); }
void Z80::bit_7_d()   { M_BIT(7, DE.B.h); }
void Z80::bit_7_e()   { M_BIT(7, DE.B.l); }
void Z80::bit_7_h()   { M_BIT(7, HL.B.h); }
void Z80::bit_7_l()   { M_BIT(7, HL.B.l); }

void Z80::call_c()  { if (M_C())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_m()  { if (M_M())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_nc() { if (M_NC()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_nz() { if (M_NZ()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_p()  { if (M_P())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_pe() { if (M_PE()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_po() { if (M_PO()) { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call_z()  { if (M_Z())  { M_CALL(); } else { M_SKIP_CALL(); } }
void Z80::call()                  { M_CALL(); }

void Z80::ccf() {
	AF.B.l = ((AF.B.l&(S_FLAG|Z_FLAG|P_FLAG|C_FLAG)|
	          ((AF.B.l&C_FLAG)<<4))|
		  XYTable[AF.B.h]                       )^1;
}

void Z80::cp_xhl() { M_CP(M_RD_XHL()); }
void Z80::cp_xix() { M_CP(M_RD_XIX()); }
void Z80::cp_xiy() { M_CP(M_RD_XIY()); }
void Z80::cp_a()   { M_CP(AF.B.h); }
void Z80::cp_b()   { M_CP(BC.B.h); }
void Z80::cp_c()   { M_CP(BC.B.l); }
void Z80::cp_d()   { M_CP(DE.B.h); }
void Z80::cp_e()   { M_CP(DE.B.l); }
void Z80::cp_h()   { M_CP(HL.B.h); }
void Z80::cp_l()   { M_CP(HL.B.l); }
void Z80::cp_ixh() { M_CP(IX.B.h); }
void Z80::cp_ixl() { M_CP(IX.B.l); }
void Z80::cp_iyh() { M_CP(IY.B.h); }
void Z80::cp_iyl() { M_CP(IY.B.l); }
void Z80::cp_byte(){ M_CP(Z80_RDMEM_OPCODE()); }

void Z80::cpd() {
	byte val = Z80_RDMEM(HL.w);
	byte res = AF.B.h - val;
	HL.w--; BC.w--;
	AF.B.l = (AF.B.l & C_FLAG)|
	         ((AF.B.h^val^res)&H_FLAG)|
	         ZSTable[res]|
	         N_FLAG;
	if (AF.B.l & H_FLAG) res -= 1;
	if (res & 0x02) AF.B.l |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) AF.B.l |= X_FLAG; // bit 3 -> flag 3
	if (BC.w) AF.B.l |= V_FLAG;
}
void Z80::cpdr() {
	cpd ();
	if (BC.w && !(AF.B.l&Z_FLAG)) { ICount+=5; PC.w-=2; }
}

void Z80::cpi() {
	byte val = Z80_RDMEM(HL.w);
	byte res = AF.B.h - val;
	HL.w++; BC.w--;
	AF.B.l = (AF.B.l & C_FLAG)|
	         ((AF.B.h^val^res)&H_FLAG)|
	         ZSTable[res]|
	         N_FLAG;
	if (AF.B.l & H_FLAG) res -= 1;
	if (res & 0x02) AF.B.l |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) AF.B.l |= X_FLAG; // bit 3 -> flag 3
	if (BC.w) AF.B.l |= V_FLAG;
}
void Z80::cpir() {
	cpi ();
	if (BC.w && !(AF.B.l&Z_FLAG)) { ICount+=5; PC.w-=2; }
}

void Z80::cpl() { AF.B.h^=0xFF; AF.B.l|=(H_FLAG|N_FLAG); }

void Z80::daa() {
	int i = AF.B.h;
	if (AF.B.l&C_FLAG) i|=256;
	if (AF.B.l&H_FLAG) i|=512;
	if (AF.B.l&N_FLAG) i|=1024;
	AF.w = DAATable[i];
}

void Z80::dec_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_DEC(i);
	Z80_WRMEM(HL.w, i);
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
void Z80::dec_a()   { M_DEC(AF.B.h); }
void Z80::dec_b()   { M_DEC(BC.B.h); }
void Z80::dec_c()   { M_DEC(BC.B.l); }
void Z80::dec_d()   { M_DEC(DE.B.h); }
void Z80::dec_e()   { M_DEC(DE.B.l); }
void Z80::dec_h()   { M_DEC(HL.B.h); }
void Z80::dec_l()   { M_DEC(HL.B.l); }
void Z80::dec_ixh() { M_DEC(IX.B.h); }
void Z80::dec_ixl() { M_DEC(IX.B.l); }
void Z80::dec_iyh() { M_DEC(IY.B.h); }
void Z80::dec_iyl() { M_DEC(IY.B.l); }

void Z80::dec_bc() { --BC.w; }
void Z80::dec_de() { --DE.w; }
void Z80::dec_hl() { --HL.w; }
void Z80::dec_ix() { --IX.w; }
void Z80::dec_iy() { --IY.w; }
void Z80::dec_sp() { --SP.w; }

void Z80::di() { IFF1 = nextIFF1 = IFF2 = false; }

void Z80::djnz() { if (--BC.B.h) { M_JR(); } else { M_SKIP_JR(); } }

void Z80::ex_xsp_hl() {
	int i = Z80_RDMEM_WORD(SP.w);
	Z80_WRMEM_WORD(SP.w, HL.w);
	HL.w = i;
}
void Z80::ex_xsp_ix() {
	int i = Z80_RDMEM_WORD(SP.w);
	Z80_WRMEM_WORD(SP.w, IX.w);
	IX.w = i;
}
void Z80::ex_xsp_iy()
{
	int i = Z80_RDMEM_WORD(SP.w);
	Z80_WRMEM_WORD(SP.w, IY.w);
	IY.w = i;
}
void Z80::ex_af_af() {
	int i = AF.w;
	AF.w = AF2.w;
	AF2.w = i;
}
void Z80::ex_de_hl() {
	int i = DE.w;
	DE.w = HL.w;
	HL.w = i;
}
void Z80::exx() {
	int i;
	i = BC.w; BC.w = BC2.w; BC2.w = i;
	i = DE.w; DE.w = DE2.w; DE2.w = i;
	i = HL.w; HL.w = HL2.w; HL2.w = i;
}

void Z80::halt() {
	HALT = true;
}

void Z80::im_0() { IM = 0; }
void Z80::im_1() { IM = 1; }
void Z80::im_2() { IM = 2; }

void Z80::in_a_c() { M_IN(AF.B.h); }
void Z80::in_b_c() { M_IN(BC.B.h); }
void Z80::in_c_c() { M_IN(BC.B.l); }
void Z80::in_d_c() { M_IN(DE.B.h); }
void Z80::in_e_c() { M_IN(DE.B.l); }
void Z80::in_h_c() { M_IN(HL.B.h); }
void Z80::in_l_c() { M_IN(HL.B.l); }
void Z80::in_0_c() { byte i; M_IN(i); } //discard result
void Z80::in_a_byte() { AF.B.h = Z80_In(Z80_RDMEM_OPCODE()+(AF.B.h<<8)); }

void Z80::inc_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_INC(i);
	Z80_WRMEM(HL.w, i);
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
void Z80::inc_a()   { M_INC(AF.B.h); }
void Z80::inc_b()   { M_INC(BC.B.h); }
void Z80::inc_c()   { M_INC(BC.B.l); }
void Z80::inc_d()   { M_INC(DE.B.h); }
void Z80::inc_e()   { M_INC(DE.B.l); }
void Z80::inc_h()   { M_INC(HL.B.h); }
void Z80::inc_l()   { M_INC(HL.B.l); }
void Z80::inc_ixh() { M_INC(IX.B.h); }
void Z80::inc_ixl() { M_INC(IX.B.l); }
void Z80::inc_iyh() { M_INC(IY.B.h); }
void Z80::inc_iyl() { M_INC(IY.B.l); }

void Z80::inc_bc() { ++BC.w; }
void Z80::inc_de() { ++DE.w; }
void Z80::inc_hl() { ++HL.w; }
void Z80::inc_ix() { ++IX.w; }
void Z80::inc_iy() { ++IY.w; }
void Z80::inc_sp() { ++SP.w; }

void Z80::ind() {
	byte io = Z80_In(BC.w);
	BC.B.h--;
	Z80_WRMEM(HL.w, io);
	HL.w--;
	AF.B.l = ZSTable[BC.B.h];
	if (io&S_FLAG) AF.B.l |= N_FLAG;
	if ((((BC.B.l+1)&0xff)+io)&0x100) AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[BC.B.l&3][io&3] ^
	    breg_tmp2[BC.B.h] ^
	    (BC.B.l>>2) ^
	    (io>>2)                     )&1)
		AF.B.l |= P_FLAG;
}
void Z80::indr() {
	ind ();
	if (BC.B.h) { ICount += 5; PC.w -= 2; }
}

void Z80::ini() {
	byte io = Z80_In(BC.w);
	BC.B.h--;
	Z80_WRMEM(HL.w, io);
	HL.w++;
	AF.B.l = ZSTable[BC.B.h];
	if (io&S_FLAG) AF.B.l |= N_FLAG;
	if ((((BC.B.l+1)&0xff)+io)&0x100) AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[BC.B.l&3][io&3] ^
	    breg_tmp2[BC.B.h] ^
	    (BC.B.l>>2) ^
	    (io>>2)                     )&1)
		AF.B.l |= P_FLAG;
}
void Z80::inir() {
	ini ();
	if (BC.B.h) { ICount += 5; PC.w -= 2; }
}

void Z80::jp_hl() { PC.w = HL.w; }
void Z80::jp_ix() { PC.w = IX.w; }
void Z80::jp_iy() { PC.w = IY.w; }
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

void Z80::ld_xbc_a()   { Z80_WRMEM(BC.w, AF.B.h); }
void Z80::ld_xde_a()   { Z80_WRMEM(DE.w, AF.B.h); }
void Z80::ld_xhl_a()   { Z80_WRMEM(HL.w, AF.B.h); }
void Z80::ld_xhl_b()   { Z80_WRMEM(HL.w, BC.B.h); }
void Z80::ld_xhl_c()   { Z80_WRMEM(HL.w, BC.B.l); }
void Z80::ld_xhl_d()   { Z80_WRMEM(HL.w, DE.B.h); }
void Z80::ld_xhl_e()   { Z80_WRMEM(HL.w, DE.B.l); }
void Z80::ld_xhl_h()   { Z80_WRMEM(HL.w, HL.B.h); }
void Z80::ld_xhl_l()   { Z80_WRMEM(HL.w, HL.B.l); }
void Z80::ld_xhl_byte(){ Z80_WRMEM(HL.w, Z80_RDMEM_OPCODE()); }
void Z80::ld_xix_a()   { M_WR_XIX(AF.B.h); }
void Z80::ld_xix_b()   { M_WR_XIX(BC.B.h); }
void Z80::ld_xix_c()   { M_WR_XIX(BC.B.l); }
void Z80::ld_xix_d()   { M_WR_XIX(DE.B.h); }
void Z80::ld_xix_e()   { M_WR_XIX(DE.B.l); }
void Z80::ld_xix_h()   { M_WR_XIX(HL.B.h); }
void Z80::ld_xix_l()   { M_WR_XIX(HL.B.l); }
void Z80::ld_xix_byte(){ Z80_WRMEM(M_XIX(), Z80_RDMEM_OPCODE()); }
void Z80::ld_xiy_a()   { M_WR_XIY(AF.B.h); }
void Z80::ld_xiy_b()   { M_WR_XIY(BC.B.h); }
void Z80::ld_xiy_c()   { M_WR_XIY(BC.B.l); }
void Z80::ld_xiy_d()   { M_WR_XIY(DE.B.h); }
void Z80::ld_xiy_e()   { M_WR_XIY(DE.B.l); }
void Z80::ld_xiy_h()   { M_WR_XIY(HL.B.h); }
void Z80::ld_xiy_l()   { M_WR_XIY(HL.B.l); }
void Z80::ld_xiy_byte(){ Z80_WRMEM(M_XIY(), Z80_RDMEM_OPCODE()); }
void Z80::ld_xbyte_a() { Z80_WRMEM(Z80_RDMEM_OPCODE_WORD(), AF.B.h); }
void Z80::ld_xword_bc(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), BC.w); }
void Z80::ld_xword_de(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), DE.w); }
void Z80::ld_xword_hl(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), HL.w); }
void Z80::ld_xword_ix(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), IX.w); }
void Z80::ld_xword_iy(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), IY.w); }
void Z80::ld_xword_sp(){ Z80_WRMEM_WORD(Z80_RDMEM_OPCODE_WORD(), SP.w); }
void Z80::ld_a_xbc()   { AF.B.h = Z80_RDMEM(BC.w); }
void Z80::ld_a_xde()   { AF.B.h = Z80_RDMEM(DE.w); }
void Z80::ld_a_xhl()   { AF.B.h = M_RD_XHL(); }
void Z80::ld_a_xix()   { AF.B.h = M_RD_XIX(); }
void Z80::ld_a_xiy()   { AF.B.h = M_RD_XIY(); }
void Z80::ld_a_xbyte() { AF.B.h = Z80_RDMEM(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_a_byte()  { AF.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_b_byte()  { BC.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_c_byte()  { BC.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_d_byte()  { DE.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_e_byte()  { DE.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_h_byte()  { HL.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_l_byte()  { HL.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_ixh_byte(){ IX.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_ixl_byte(){ IX.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_iyh_byte(){ IY.B.h = Z80_RDMEM_OPCODE(); }
void Z80::ld_iyl_byte(){ IY.B.l = Z80_RDMEM_OPCODE(); }
void Z80::ld_b_xhl()   { BC.B.h = M_RD_XHL(); }
void Z80::ld_c_xhl()   { BC.B.l = M_RD_XHL(); }
void Z80::ld_d_xhl()   { DE.B.h = M_RD_XHL(); }
void Z80::ld_e_xhl()   { DE.B.l = M_RD_XHL(); }
void Z80::ld_h_xhl()   { HL.B.h = M_RD_XHL(); }
void Z80::ld_l_xhl()   { HL.B.l = M_RD_XHL(); }
void Z80::ld_b_xix()   { BC.B.h = M_RD_XIX(); }
void Z80::ld_c_xix()   { BC.B.l = M_RD_XIX(); }
void Z80::ld_d_xix()   { DE.B.h = M_RD_XIX(); }
void Z80::ld_e_xix()   { DE.B.l = M_RD_XIX(); }
void Z80::ld_h_xix()   { HL.B.h = M_RD_XIX(); }
void Z80::ld_l_xix()   { HL.B.l = M_RD_XIX(); }
void Z80::ld_b_xiy()   { BC.B.h = M_RD_XIY(); }
void Z80::ld_c_xiy()   { BC.B.l = M_RD_XIY(); }
void Z80::ld_d_xiy()   { DE.B.h = M_RD_XIY(); }
void Z80::ld_e_xiy()   { DE.B.l = M_RD_XIY(); }
void Z80::ld_h_xiy()   { HL.B.h = M_RD_XIY(); }
void Z80::ld_l_xiy()   { HL.B.l = M_RD_XIY(); }
void Z80::ld_a_a()     { }
void Z80::ld_a_b()     { AF.B.h = BC.B.h; }
void Z80::ld_a_c()     { AF.B.h = BC.B.l; }
void Z80::ld_a_d()     { AF.B.h = DE.B.h; }
void Z80::ld_a_e()     { AF.B.h = DE.B.l; }
void Z80::ld_a_h()     { AF.B.h = HL.B.h; }
void Z80::ld_a_l()     { AF.B.h = HL.B.l; }
void Z80::ld_a_ixh()   { AF.B.h = IX.B.h; }
void Z80::ld_a_ixl()   { AF.B.h = IX.B.l; }
void Z80::ld_a_iyh()   { AF.B.h = IY.B.h; }
void Z80::ld_a_iyl()   { AF.B.h = IY.B.l; }
void Z80::ld_b_b()     { }
void Z80::ld_b_a()     { BC.B.h = AF.B.h; }
void Z80::ld_b_c()     { BC.B.h = BC.B.l; }
void Z80::ld_b_d()     { BC.B.h = DE.B.h; }
void Z80::ld_b_e()     { BC.B.h = DE.B.l; }
void Z80::ld_b_h()     { BC.B.h = HL.B.h; }
void Z80::ld_b_l()     { BC.B.h = HL.B.l; }
void Z80::ld_b_ixh()   { BC.B.h = IX.B.h; }
void Z80::ld_b_ixl()   { BC.B.h = IX.B.l; }
void Z80::ld_b_iyh()   { BC.B.h = IY.B.h; }
void Z80::ld_b_iyl()   { BC.B.h = IY.B.l; }
void Z80::ld_c_c()     { }
void Z80::ld_c_a()     { BC.B.l = AF.B.h; }
void Z80::ld_c_b()     { BC.B.l = BC.B.h; }
void Z80::ld_c_d()     { BC.B.l = DE.B.h; }
void Z80::ld_c_e()     { BC.B.l = DE.B.l; }
void Z80::ld_c_h()     { BC.B.l = HL.B.h; }
void Z80::ld_c_l()     { BC.B.l = HL.B.l; }
void Z80::ld_c_ixh()   { BC.B.l = IX.B.h; }
void Z80::ld_c_ixl()   { BC.B.l = IX.B.l; }
void Z80::ld_c_iyh()   { BC.B.l = IY.B.h; }
void Z80::ld_c_iyl()   { BC.B.l = IY.B.l; }
void Z80::ld_d_d()     { }
void Z80::ld_d_a()     { DE.B.h = AF.B.h; }
void Z80::ld_d_c()     { DE.B.h = BC.B.l; }
void Z80::ld_d_b()     { DE.B.h = BC.B.h; }
void Z80::ld_d_e()     { DE.B.h = DE.B.l; }
void Z80::ld_d_h()     { DE.B.h = HL.B.h; }
void Z80::ld_d_l()     { DE.B.h = HL.B.l; }
void Z80::ld_d_ixh()   { DE.B.h = IX.B.h; }
void Z80::ld_d_ixl()   { DE.B.h = IX.B.l; }
void Z80::ld_d_iyh()   { DE.B.h = IY.B.h; }
void Z80::ld_d_iyl()   { DE.B.h = IY.B.l; }
void Z80::ld_e_e()     { }
void Z80::ld_e_a()     { DE.B.l = AF.B.h; }
void Z80::ld_e_c()     { DE.B.l = BC.B.l; }
void Z80::ld_e_b()     { DE.B.l = BC.B.h; }
void Z80::ld_e_d()     { DE.B.l = DE.B.h; }
void Z80::ld_e_h()     { DE.B.l = HL.B.h; }
void Z80::ld_e_l()     { DE.B.l = HL.B.l; }
void Z80::ld_e_ixh()   { DE.B.l = IX.B.h; }
void Z80::ld_e_ixl()   { DE.B.l = IX.B.l; }
void Z80::ld_e_iyh()   { DE.B.l = IY.B.h; }
void Z80::ld_e_iyl()   { DE.B.l = IY.B.l; }
void Z80::ld_h_h()     { }
void Z80::ld_h_a()     { HL.B.h = AF.B.h; }
void Z80::ld_h_c()     { HL.B.h = BC.B.l; }
void Z80::ld_h_b()     { HL.B.h = BC.B.h; }
void Z80::ld_h_e()     { HL.B.h = DE.B.l; }
void Z80::ld_h_d()     { HL.B.h = DE.B.h; }
void Z80::ld_h_l()     { HL.B.h = HL.B.l; }
void Z80::ld_l_l()     { }
void Z80::ld_l_a()     { HL.B.l = AF.B.h; }
void Z80::ld_l_c()     { HL.B.l = BC.B.l; }
void Z80::ld_l_b()     { HL.B.l = BC.B.h; }
void Z80::ld_l_e()     { HL.B.l = DE.B.l; }
void Z80::ld_l_d()     { HL.B.l = DE.B.h; }
void Z80::ld_l_h()     { HL.B.l = HL.B.h; }
void Z80::ld_ixh_a()   { IX.B.h = AF.B.h; }
void Z80::ld_ixh_b()   { IX.B.h = BC.B.h; }
void Z80::ld_ixh_c()   { IX.B.h = BC.B.l; }
void Z80::ld_ixh_d()   { IX.B.h = DE.B.h; }
void Z80::ld_ixh_e()   { IX.B.h = DE.B.l; }
void Z80::ld_ixh_ixh() { }
void Z80::ld_ixh_ixl() { IX.B.h = IX.B.l; }
void Z80::ld_ixl_a()   { IX.B.l = AF.B.h; }
void Z80::ld_ixl_b()   { IX.B.l = BC.B.h; }
void Z80::ld_ixl_c()   { IX.B.l = BC.B.l; }
void Z80::ld_ixl_d()   { IX.B.l = DE.B.h; }
void Z80::ld_ixl_e()   { IX.B.l = DE.B.l; }
void Z80::ld_ixl_ixh() { IX.B.l = IX.B.h; }
void Z80::ld_ixl_ixl() { }
void Z80::ld_iyh_a()   { IY.B.h = AF.B.h; }
void Z80::ld_iyh_b()   { IY.B.h = BC.B.h; }
void Z80::ld_iyh_c()   { IY.B.h = BC.B.l; }
void Z80::ld_iyh_d()   { IY.B.h = DE.B.h; }
void Z80::ld_iyh_e()   { IY.B.h = DE.B.l; }
void Z80::ld_iyh_iyh() { }
void Z80::ld_iyh_iyl() { IY.B.h = IY.B.l; }
void Z80::ld_iyl_a()   { IY.B.l = AF.B.h; }
void Z80::ld_iyl_b()   { IY.B.l = BC.B.h; }
void Z80::ld_iyl_c()   { IY.B.l = BC.B.l; }
void Z80::ld_iyl_d()   { IY.B.l = DE.B.h; }
void Z80::ld_iyl_e()   { IY.B.l = DE.B.l; }
void Z80::ld_iyl_iyh() { IY.B.l = IY.B.h; }
void Z80::ld_iyl_iyl() { }
void Z80::ld_bc_xword(){ BC.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_bc_word() { BC.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_de_xword(){ DE.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_de_word() { DE.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_hl_xword(){ HL.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_hl_word() { HL.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_ix_xword(){ IX.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_ix_word() { IX.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_iy_xword(){ IY.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_iy_word() { IY.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_xword(){ SP.w = Z80_RDMEM_WORD(Z80_RDMEM_OPCODE_WORD()); }
void Z80::ld_sp_word() { SP.w = Z80_RDMEM_OPCODE_WORD(); }
void Z80::ld_sp_hl()   { SP.w = HL.w; }
void Z80::ld_sp_ix()   { SP.w = IX.w; }
void Z80::ld_sp_iy()   { SP.w = IY.w; }
void Z80::ld_a_i() {
	AF.B.h = I;
	AF.B.l = (AF.B.l&C_FLAG)|ZSXYTable[I]|((IFF2 ? V_FLAG : 0));
}
void Z80::ld_i_a()     { I = AF.B.h; }
void Z80::ld_a_r() {
	AF.B.h=(R&127)|(R2&128);
	AF.B.l=(AF.B.l&C_FLAG)|ZSXYTable[AF.B.h]|((IFF2 ? V_FLAG : 0));
}
void Z80::ld_r_a()     { R = R2 = AF.B.h; }

void Z80::ldd() {
	byte io = Z80_RDMEM(HL.w);
	Z80_WRMEM(DE.w, io);        
	AF.B.l &= S_FLAG | Z_FLAG | C_FLAG;
	if ((AF.B.h+io)&0x02) AF.B.l |= Y_FLAG;	// bit 1 -> flag 5
	if ((AF.B.h+io)&0x08) AF.B.l |= X_FLAG;	// bit 3 -> flag 3
	HL.w--; DE.w--; BC.w--;
	if (BC.w) AF.B.l |= V_FLAG;
}
void Z80::lddr() {
	ldd ();
	if (BC.w) { ICount += 5; PC.w -= 2; }
}
void Z80::ldi() {
	byte io = Z80_RDMEM(HL.w);
	Z80_WRMEM(DE.w, io);        
	AF.B.l &= S_FLAG | Z_FLAG | C_FLAG;
	if ((AF.B.h+io)&0x02) AF.B.l |= Y_FLAG;	// bit 1 -> flag 5
	if ((AF.B.h+io)&0x08) AF.B.l |= X_FLAG;	// bit 3 -> flag 3
	HL.w++; DE.w++; BC.w--;
	if (BC.w) AF.B.l |= V_FLAG;
}
void Z80::ldir() {
	ldi ();
	if (BC.w) { ICount += 5; PC.w -= 2; }
}

void Z80::neg() {
	 byte i = AF.B.h;
	 AF.B.h = 0;
	 M_SUB(i);
}

void Z80::nop() { };

void Z80::or_xhl() { M_OR(M_RD_XHL()); }
void Z80::or_xix() { M_OR(M_RD_XIX()); }
void Z80::or_xiy() { M_OR(M_RD_XIY()); }
void Z80::or_a()   { AF.B.l = ZSPXYTable[AF.B.h]; }
void Z80::or_b()   { M_OR(BC.B.h); }
void Z80::or_c()   { M_OR(BC.B.l); }
void Z80::or_d()   { M_OR(DE.B.h); }
void Z80::or_e()   { M_OR(DE.B.l); }
void Z80::or_h()   { M_OR(HL.B.h); }
void Z80::or_l()   { M_OR(HL.B.l); }
void Z80::or_ixh() { M_OR(IX.B.h); }
void Z80::or_ixl() { M_OR(IX.B.l); }
void Z80::or_iyh() { M_OR(IY.B.h); }
void Z80::or_iyl() { M_OR(IY.B.l); }
void Z80::or_byte(){ M_OR(Z80_RDMEM_OPCODE()); }

void Z80::outd() {
	byte io = Z80_RDMEM(HL.w--);
	BC.B.h--;
	Z80_Out(BC.w, io);
	AF.B.l = ZSTable[BC.B.h];
	if (io&S_FLAG) AF.B.l |= N_FLAG;
	if ((((BC.B.l+1)&0xff)+io)&0x100) AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[BC.B.l&3][io&3] ^
	    breg_tmp2[BC.B.h] ^
	    (BC.B.l>>2) ^
	    (io>>2))                    & 1 )
		AF.B.l |= P_FLAG;
}
void Z80::otdr() {
	outd ();
	if (BC.B.h) { ICount += 5; PC.w -= 2; }
}
void Z80::outi() {
	byte io = Z80_RDMEM(HL.w++);
	BC.B.h--;
	Z80_Out(BC.w, io);
	AF.B.l = ZSTable[BC.B.h];
	if (io&S_FLAG) AF.B.l |= N_FLAG;
	if ((((BC.B.l+1)&0xff)+io)&0x100) AF.B.l |= H_FLAG | C_FLAG;
	if ((irep_tmp1[BC.B.l&3][io&3] ^
	    breg_tmp2[BC.B.h] ^
	    (BC.B.l>>2) ^
	    (io>>2))                    & 1 )
		AF.B.l |= P_FLAG;
}
void Z80::otir() {
	outi ();
	if (BC.B.h) { ICount += 5; PC.w -= 2; }
}

void Z80::out_c_a()   { Z80_Out(BC.w, AF.B.h); }
void Z80::out_c_b()   { Z80_Out(BC.w, BC.B.h); }
void Z80::out_c_c()   { Z80_Out(BC.w, BC.B.l); }
void Z80::out_c_d()   { Z80_Out(BC.w, DE.B.h); }
void Z80::out_c_e()   { Z80_Out(BC.w, DE.B.l); }
void Z80::out_c_h()   { Z80_Out(BC.w, HL.B.h); }
void Z80::out_c_l()   { Z80_Out(BC.w, HL.B.l); }
void Z80::out_c_0()   { Z80_Out(BC.w, 0); }
void Z80::out_byte_a(){ Z80_Out(Z80_RDMEM_OPCODE()+(AF.B.h<<8), AF.B.h); }

void Z80::pop_af() { M_POP(AF.w); }
void Z80::pop_bc() { M_POP(BC.w); }
void Z80::pop_de() { M_POP(DE.w); }
void Z80::pop_hl() { M_POP(HL.w); }
void Z80::pop_ix() { M_POP(IX.w); }
void Z80::pop_iy() { M_POP(IY.w); }

void Z80::push_af() { M_PUSH(AF.w); }
void Z80::push_bc() { M_PUSH(BC.w); }
void Z80::push_de() { M_PUSH(DE.w); }
void Z80::push_hl() { M_PUSH(HL.w); }
void Z80::push_ix() { M_PUSH(IX.w); }
void Z80::push_iy() { M_PUSH(IY.w); }

void Z80::res_0_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(0, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(0, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_0_a() { M_RES(0, AF.B.h); };
void Z80::res_0_b() { M_RES(0, BC.B.h); };
void Z80::res_0_c() { M_RES(0, BC.B.l); };
void Z80::res_0_d() { M_RES(0, DE.B.h); };
void Z80::res_0_e() { M_RES(0, DE.B.l); };
void Z80::res_0_h() { M_RES(0, HL.B.h); };
void Z80::res_0_l() { M_RES(0, HL.B.l); };

void Z80::res_1_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(1, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(1, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_1_a() { M_RES(1, AF.B.h); };
void Z80::res_1_b() { M_RES(1, BC.B.h); };
void Z80::res_1_c() { M_RES(1, BC.B.l); };
void Z80::res_1_d() { M_RES(1, DE.B.h); };
void Z80::res_1_e() { M_RES(1, DE.B.l); };
void Z80::res_1_h() { M_RES(1, HL.B.h); };
void Z80::res_1_l() { M_RES(1, HL.B.l); };

void Z80::res_2_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(2, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(2, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_2_a() { M_RES(2, AF.B.h); };
void Z80::res_2_b() { M_RES(2, BC.B.h); };
void Z80::res_2_c() { M_RES(2, BC.B.l); };
void Z80::res_2_d() { M_RES(2, DE.B.h); };
void Z80::res_2_e() { M_RES(2, DE.B.l); };
void Z80::res_2_h() { M_RES(2, HL.B.h); };
void Z80::res_2_l() { M_RES(2, HL.B.l); };

void Z80::res_3_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(3, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(3, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_3_a() { M_RES(3, AF.B.h); };
void Z80::res_3_b() { M_RES(3, BC.B.h); };
void Z80::res_3_c() { M_RES(3, BC.B.l); };
void Z80::res_3_d() { M_RES(3, DE.B.h); };
void Z80::res_3_e() { M_RES(3, DE.B.l); };
void Z80::res_3_h() { M_RES(3, HL.B.h); };
void Z80::res_3_l() { M_RES(3, HL.B.l); };

void Z80::res_4_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(4, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(4, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_4_a() { M_RES(4, AF.B.h); };
void Z80::res_4_b() { M_RES(4, BC.B.h); };
void Z80::res_4_c() { M_RES(4, BC.B.l); };
void Z80::res_4_d() { M_RES(4, DE.B.h); };
void Z80::res_4_e() { M_RES(4, DE.B.l); };
void Z80::res_4_h() { M_RES(4, HL.B.h); };
void Z80::res_4_l() { M_RES(4, HL.B.l); };

void Z80::res_5_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(5, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(5, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_5_a() { M_RES(5, AF.B.h); };
void Z80::res_5_b() { M_RES(5, BC.B.h); };
void Z80::res_5_c() { M_RES(5, BC.B.l); };
void Z80::res_5_d() { M_RES(5, DE.B.h); };
void Z80::res_5_e() { M_RES(5, DE.B.l); };
void Z80::res_5_h() { M_RES(5, HL.B.h); };
void Z80::res_5_l() { M_RES(5, HL.B.l); };

void Z80::res_6_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(6, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(6, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_6_a() { M_RES(6, AF.B.h); };
void Z80::res_6_b() { M_RES(6, BC.B.h); };
void Z80::res_6_c() { M_RES(6, BC.B.l); };
void Z80::res_6_d() { M_RES(6, DE.B.h); };
void Z80::res_6_e() { M_RES(6, DE.B.l); };
void Z80::res_6_h() { M_RES(6, HL.B.h); };
void Z80::res_6_l() { M_RES(6, HL.B.l); };

void Z80::res_7_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RES(7, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::res_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RES(7, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::res_7_a() { M_RES(7, AF.B.h); };
void Z80::res_7_b() { M_RES(7, BC.B.h); };
void Z80::res_7_c() { M_RES(7, BC.B.l); };
void Z80::res_7_d() { M_RES(7, DE.B.h); };
void Z80::res_7_e() { M_RES(7, DE.B.l); };
void Z80::res_7_h() { M_RES(7, HL.B.h); };
void Z80::res_7_l() { M_RES(7, HL.B.l); };

void Z80::ret()                  { M_RET(); }
void Z80::ret_c()  { if (M_C())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_m()  { if (M_M())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_nc() { if (M_NC()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_nz() { if (M_NZ()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_p()  { if (M_P())  { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_pe() { if (M_PE()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_po() { if (M_PO()) { M_RET(); } else { M_SKIP_RET(); } }
void Z80::ret_z()  { if (M_Z())  { M_RET(); } else { M_SKIP_RET(); } }

void Z80::reti() { IFF1 = nextIFF1 = IFF2; interface->Z80_Reti(); M_RET(); }	// same as retn!!
void Z80::retn() { IFF1 = nextIFF1 = IFF2; interface->Z80_Retn(); M_RET(); }

void Z80::rl_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RL(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::rl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rl_a() { M_RL(AF.B.h); }
void Z80::rl_b() { M_RL(BC.B.h); }
void Z80::rl_c() { M_RL(BC.B.l); }
void Z80::rl_d() { M_RL(DE.B.h); }
void Z80::rl_e() { M_RL(DE.B.l); }
void Z80::rl_h() { M_RL(HL.B.h); }
void Z80::rl_l() { M_RL(HL.B.l); }
void Z80::rla()  { M_RLA(); }

void Z80::rlc_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RLC(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::rlc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}

void Z80::rlc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RLC(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rlc_a() { M_RLC(AF.B.h); }
void Z80::rlc_b() { M_RLC(BC.B.h); }
void Z80::rlc_c() { M_RLC(BC.B.l); }
void Z80::rlc_d() { M_RLC(DE.B.h); }
void Z80::rlc_e() { M_RLC(DE.B.l); }
void Z80::rlc_h() { M_RLC(HL.B.h); }
void Z80::rlc_l() { M_RLC(HL.B.l); }
void Z80::rlca()  { M_RLCA(); }

void Z80::rld() {
	byte i = Z80_RDMEM(HL.w);
	Z80_WRMEM(HL.w, (i<<4)|(AF.B.h&0x0F));
	AF.B.h = (AF.B.h&0xF0)|(i>>4);
	AF.B.l = (AF.B.l&C_FLAG)|ZSPXYTable[AF.B.h];
}

void Z80::rr_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RR(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::rr_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RR(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rr_a() { M_RR(AF.B.h); }
void Z80::rr_b() { M_RR(BC.B.h); }
void Z80::rr_c() { M_RR(BC.B.l); }
void Z80::rr_d() { M_RR(DE.B.h); }
void Z80::rr_e() { M_RR(DE.B.l); }
void Z80::rr_h() { M_RR(HL.B.h); }
void Z80::rr_l() { M_RR(HL.B.l); }
void Z80::rra()  { M_RRA(); }

void Z80::rrc_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_RRC(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::rrc_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_RRC(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::rrc_a() { M_RRC(AF.B.h); }
void Z80::rrc_b() { M_RRC(BC.B.h); }
void Z80::rrc_c() { M_RRC(BC.B.l); }
void Z80::rrc_d() { M_RRC(DE.B.h); }
void Z80::rrc_e() { M_RRC(DE.B.l); }
void Z80::rrc_h() { M_RRC(HL.B.h); }
void Z80::rrc_l() { M_RRC(HL.B.l); }
void Z80::rrca()  { M_RRCA(); }

void Z80::rrd() {
	byte i = Z80_RDMEM(HL.w);
	Z80_WRMEM(HL.w, (i>>4)|(AF.B.h<<4));
	AF.B.h = (AF.B.h&0xF0)|(i&0x0F);
	AF.B.l = (AF.B.l&C_FLAG)|ZSPXYTable[AF.B.h];
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
void Z80::sbc_a_a()   { M_SBC(AF.B.h); }
void Z80::sbc_a_b()   { M_SBC(BC.B.h); }
void Z80::sbc_a_c()   { M_SBC(BC.B.l); }
void Z80::sbc_a_d()   { M_SBC(DE.B.h); }
void Z80::sbc_a_e()   { M_SBC(DE.B.l); }
void Z80::sbc_a_h()   { M_SBC(HL.B.h); }
void Z80::sbc_a_l()   { M_SBC(HL.B.l); }
void Z80::sbc_a_ixh() { M_SBC(IX.B.h); }
void Z80::sbc_a_ixl() { M_SBC(IX.B.l); }
void Z80::sbc_a_iyh() { M_SBC(IY.B.h); }
void Z80::sbc_a_iyl() { M_SBC(IY.B.l); }

void Z80::sbc_hl_bc() { M_SBCW(BC.w); }
void Z80::sbc_hl_de() { M_SBCW(DE.w); }
void Z80::sbc_hl_hl() { M_SBCW(HL.w); }
void Z80::sbc_hl_sp() { M_SBCW(SP.w); }

void Z80::scf() { 
	AF.B.l = (AF.B.l&(S_FLAG|Z_FLAG|P_FLAG))|
	         C_FLAG|
	         XYTable[AF.B.h];
}

void Z80::set_0_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(0, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_0_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(0, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_0_a() { M_SET(0, AF.B.h); }
void Z80::set_0_b() { M_SET(0, BC.B.h); }
void Z80::set_0_c() { M_SET(0, BC.B.l); }
void Z80::set_0_d() { M_SET(0, DE.B.h); }
void Z80::set_0_e() { M_SET(0, DE.B.l); }
void Z80::set_0_h() { M_SET(0, HL.B.h); }
void Z80::set_0_l() { M_SET(0, HL.B.l); }

void Z80::set_1_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(1, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_1_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(1, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_1_a() { M_SET(1, AF.B.h); }
void Z80::set_1_b() { M_SET(1, BC.B.h); }
void Z80::set_1_c() { M_SET(1, BC.B.l); }
void Z80::set_1_d() { M_SET(1, DE.B.h); }
void Z80::set_1_e() { M_SET(1, DE.B.l); }
void Z80::set_1_h() { M_SET(1, HL.B.h); }
void Z80::set_1_l() { M_SET(1, HL.B.l); }

void Z80::set_2_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(2, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_2_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(2, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_2_a() { M_SET(2, AF.B.h); }
void Z80::set_2_b() { M_SET(2, BC.B.h); }
void Z80::set_2_c() { M_SET(2, BC.B.l); }
void Z80::set_2_d() { M_SET(2, DE.B.h); }
void Z80::set_2_e() { M_SET(2, DE.B.l); }
void Z80::set_2_h() { M_SET(2, HL.B.h); }
void Z80::set_2_l() { M_SET(2, HL.B.l); }

void Z80::set_3_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(3, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_3_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(3, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_3_a() { M_SET(3, AF.B.h); }
void Z80::set_3_b() { M_SET(3, BC.B.h); }
void Z80::set_3_c() { M_SET(3, BC.B.l); }
void Z80::set_3_d() { M_SET(3, DE.B.h); }
void Z80::set_3_e() { M_SET(3, DE.B.l); }
void Z80::set_3_h() { M_SET(3, HL.B.h); }
void Z80::set_3_l() { M_SET(3, HL.B.l); }

void Z80::set_4_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(4, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_4_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(4, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_4_a() { M_SET(4, AF.B.h); }
void Z80::set_4_b() { M_SET(4, BC.B.h); }
void Z80::set_4_c() { M_SET(4, BC.B.l); }
void Z80::set_4_d() { M_SET(4, DE.B.h); }
void Z80::set_4_e() { M_SET(4, DE.B.l); }
void Z80::set_4_h() { M_SET(4, HL.B.h); }
void Z80::set_4_l() { M_SET(4, HL.B.l); }

void Z80::set_5_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(5, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_5_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(5, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_5_a() { M_SET(5, AF.B.h); }
void Z80::set_5_b() { M_SET(5, BC.B.h); }
void Z80::set_5_c() { M_SET(5, BC.B.l); }
void Z80::set_5_d() { M_SET(5, DE.B.h); }
void Z80::set_5_e() { M_SET(5, DE.B.l); }
void Z80::set_5_h() { M_SET(5, HL.B.h); }
void Z80::set_5_l() { M_SET(5, HL.B.l); }

void Z80::set_6_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(6, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_6_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(6, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_6_a() { M_SET(6, AF.B.h); }
void Z80::set_6_b() { M_SET(6, BC.B.h); }
void Z80::set_6_c() { M_SET(6, BC.B.l); }
void Z80::set_6_d() { M_SET(6, DE.B.h); }
void Z80::set_6_e() { M_SET(6, DE.B.l); }
void Z80::set_6_h() { M_SET(6, HL.B.h); }
void Z80::set_6_l() { M_SET(6, HL.B.l); }

void Z80::set_7_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SET(7, i);
	Z80_WRMEM(HL.w, i);
}
void Z80::set_7_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SET(7, i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::set_7_a() { M_SET(7, AF.B.h); }
void Z80::set_7_b() { M_SET(7, BC.B.h); }
void Z80::set_7_c() { M_SET(7, BC.B.l); }
void Z80::set_7_d() { M_SET(7, DE.B.h); }
void Z80::set_7_e() { M_SET(7, DE.B.l); }
void Z80::set_7_h() { M_SET(7, HL.B.h); }
void Z80::set_7_l() { M_SET(7, HL.B.l); }

void Z80::sla_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SLA(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::sla_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLA(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sla_a() { M_SLA(AF.B.h); }
void Z80::sla_b() { M_SLA(BC.B.h); }
void Z80::sla_c() { M_SLA(BC.B.l); }
void Z80::sla_d() { M_SLA(DE.B.h); }
void Z80::sla_e() { M_SLA(DE.B.l); }
void Z80::sla_h() { M_SLA(HL.B.h); }
void Z80::sla_l() { M_SLA(HL.B.l); }

void Z80::sll_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SLL(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::sll_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SLL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sll_a() { M_SLL(AF.B.h); }
void Z80::sll_b() { M_SLL(BC.B.h); }
void Z80::sll_c() { M_SLL(BC.B.l); }
void Z80::sll_d() { M_SLL(DE.B.h); }
void Z80::sll_e() { M_SLL(DE.B.l); }
void Z80::sll_h() { M_SLL(HL.B.h); }
void Z80::sll_l() { M_SLL(HL.B.l); }

void Z80::sra_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SRA(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::sra_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRA(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::sra_a() { M_SRA(AF.B.h); }
void Z80::sra_b() { M_SRA(BC.B.h); }
void Z80::sra_c() { M_SRA(BC.B.l); }
void Z80::sra_d() { M_SRA(DE.B.h); }
void Z80::sra_e() { M_SRA(DE.B.l); }
void Z80::sra_h() { M_SRA(HL.B.h); }
void Z80::sra_l() { M_SRA(HL.B.l); }

void Z80::srl_xhl() {
	byte i = Z80_RDMEM(HL.w);
	M_SRL(i);
	Z80_WRMEM(HL.w, i);
}
void Z80::srl_xix() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_a() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_b() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_c() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_d() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_e() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_h() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xix_l() {
	int j = M_XIX();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_a() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	AF.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_b() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	BC.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_c() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	BC.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_d() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	DE.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_e() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	DE.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_h() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	HL.B.h = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_xiy_l() {
	int j = M_XIY();
	byte i = Z80_RDMEM(j);
	M_SRL(i);
	HL.B.l = i;
	Z80_WRMEM(j, i);
}
void Z80::srl_a() { M_SRL(AF.B.h); }
void Z80::srl_b() { M_SRL(BC.B.h); }
void Z80::srl_c() { M_SRL(BC.B.l); }
void Z80::srl_d() { M_SRL(DE.B.h); }
void Z80::srl_e() { M_SRL(DE.B.l); }
void Z80::srl_h() { M_SRL(HL.B.h); }
void Z80::srl_l() { M_SRL(HL.B.l); }

void Z80::sub_xhl() { M_SUB(M_RD_XHL()); }
void Z80::sub_xix() { M_SUB(M_RD_XIX()); }
void Z80::sub_xiy() { M_SUB(M_RD_XIY()); }
void Z80::sub_a()   { AF.w = Z_FLAG|N_FLAG; }
void Z80::sub_b()   { M_SUB(BC.B.h); }
void Z80::sub_c()   { M_SUB(BC.B.l); }
void Z80::sub_d()   { M_SUB(DE.B.h); }
void Z80::sub_e()   { M_SUB(DE.B.l); }
void Z80::sub_h()   { M_SUB(HL.B.h); }
void Z80::sub_l()   { M_SUB(HL.B.l); }
void Z80::sub_ixh() { M_SUB(IX.B.h); }
void Z80::sub_ixl() { M_SUB(IX.B.l); }
void Z80::sub_iyh() { M_SUB(IY.B.h); }
void Z80::sub_iyl() { M_SUB(IY.B.l); }
void Z80::sub_byte(){ M_SUB(Z80_RDMEM_OPCODE()); }

void Z80::xor_xhl() { M_XOR(M_RD_XHL()); }
void Z80::xor_xix() { M_XOR(M_RD_XIX()); }
void Z80::xor_xiy() { M_XOR(M_RD_XIY()); }
void Z80::xor_a()   { AF.w = Z_FLAG|V_FLAG;}
void Z80::xor_b()   { M_XOR(BC.B.h); }
void Z80::xor_c()   { M_XOR(BC.B.l); }
void Z80::xor_d()   { M_XOR(DE.B.h); }
void Z80::xor_e()   { M_XOR(DE.B.l); }
void Z80::xor_h()   { M_XOR(HL.B.h); }
void Z80::xor_l()   { M_XOR(HL.B.l); }
void Z80::xor_ixh() { M_XOR(IX.B.h); }
void Z80::xor_ixl() { M_XOR(IX.B.l); }
void Z80::xor_iyh() { M_XOR(IY.B.h); }
void Z80::xor_iyl() { M_XOR(IY.B.l); }
void Z80::xor_byte(){ M_XOR(Z80_RDMEM_OPCODE()); }

void Z80::no_op()    { --PC.w; }
void Z80::no_op_xx() { ++PC.w; }

void Z80::patch() { interface->Z80_Patch(); }



void Z80::dd_cb() {
	unsigned opcode = Z80_RDOP_ARG((PC.w+1)&0xFFFF);
	ICount += cycles_xx_cb[opcode];
	(this->*opcode_dd_cb[opcode])();
	++PC.w;
}
void Z80::fd_cb() {
	unsigned opcode = Z80_RDOP_ARG((PC.w+1)&0xFFFF);
	ICount += cycles_xx_cb[opcode];
	(this->*opcode_fd_cb[opcode])();
	++PC.w;
}
void Z80::cb() {
	++R;
	unsigned opcode = Z80_RDOP(PC.w);
	PC.w++;
	ICount += cycles_cb[opcode];
	(this->*opcode_cb[opcode])();
}
void Z80::dd() {
	++R;
	unsigned opcode = Z80_RDOP(PC.w);
	PC.w++;
	ICount += cycles_xx[opcode];
	(this->*opcode_dd[opcode])();
}
void Z80::ed() {
	++R;
	unsigned opcode = Z80_RDOP(PC.w);
	PC.w++;
	ICount += cycles_ed[opcode];
	(this->*opcode_ed[opcode])();
}
void Z80::fd () {
	++R;
	unsigned opcode = Z80_RDOP(PC.w);
	PC.w++;
	ICount += cycles_xx[opcode];
	(this->*opcode_fd[opcode])();
}

void Z80::ei() {
	nextIFF1 = true;	// delay one instruction
	IFF2 = true;
}

