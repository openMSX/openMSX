// $Id$

/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                              Z80Codes.h                              ***/
/***                                                                      ***/
/*** This file contains various macros used by the emulation engine       ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

// If we are building an R800 then we need R800CPU defined
//#define R800CPU 


static inline void M_POP(word &Rg) {
	Rg = M_RDSTACK(R.SP.W.l)+(M_RDSTACK((R.SP.W.l+1)&65535)<<8);
	R.SP.W.l += 2;
}

static inline void M_PUSH(word Rg) {
	R.SP.W.l -= 2;
	M_WRSTACK(R.SP.W.l, Rg);
	M_WRSTACK((R.SP.W.l+1)&65535, Rg>>8);
}

static inline void M_CALL() {
	word q = M_RDMEM_OPCODE_WORD();
	M_PUSH(R.PC.W.l);
	R.PC.W.l = q;
#ifdef R800CPU
	R.ICount-=2;
#else
	R.ICount-=7;
#endif
}

static inline void M_JR() {
	R.PC.W.l += ((offset)M_RDOP_ARG(R.PC.W.l))+1;
#ifdef R800CPU
	R.ICount -= 1;
#else
	R.ICount -= 5;
#endif
}

static inline void M_RET() {
	M_POP(R.PC.W.l);
#ifdef R800CPU
	R.ICount -= 2;
#else
	R.ICount -= 6;
#endif
}

static inline void M_JP() {
        R.PC.W.l = M_RDOP_ARG(R.PC.W.l)+((M_RDOP_ARG((R.PC.W.l+1)&65535))<<8);
}

static inline void M_RST(word Addr) {
	M_PUSH(R.PC.W.l);
	R.PC.W.l = Addr;
}

static inline void M_SET(int Bit, byte &Reg) { 
	Reg |= 1<<Bit;
}
static inline void M_RES(int Bit, byte &Reg) {
	Reg &= ~(1<<Bit);
}
static inline void M_BIT(int Bit, byte Reg) {
	R.AF.B.l = (R.AF.B.l&C_FLAG) | H_FLAG | 
	           ((Reg&(1<<Bit)) ? ((Bit==7) ? S_FLAG : 0) : Z_FLAG);
}

static inline void M_AND(byte Reg) {
	R.AF.B.h &= Reg;
	R.AF.B.l = ZSPTable[R.AF.B.h]|H_FLAG;
}
static inline void M_OR(byte Reg) {
	R.AF.B.h |= Reg;
	R.AF.B.l = ZSPTable[R.AF.B.h];
}
static inline void M_XOR(byte Reg) {
	R.AF.B.h ^= Reg;
	R.AF.B.l = ZSPTable[R.AF.B.h];
}

static inline void M_IN(byte &Reg) {
	Reg = DoIn(R.BC.B.l, R.BC.B.h);
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSPTable[Reg];
}

static inline void M_RLCA() {
	R.AF.B.h = (R.AF.B.h<<1)|((R.AF.B.h&0x80)>>7);
	R.AF.B.l = (R.AF.B.l&0xEC)|(R.AF.B.h&C_FLAG);
}
static inline void M_RRCA() {
	R.AF.B.l = (R.AF.B.l&0xEC)|(R.AF.B.h&0x01);
	R.AF.B.h = (R.AF.B.h>>1)|(R.AF.B.h<<7);
}
static inline void M_RLA() {
	byte i = R.AF.B.l&C_FLAG;
	R.AF.B.l = (R.AF.B.l&0xEC)|((R.AF.B.h&0x80)>>7);
	R.AF.B.h = (R.AF.B.h<<1)|i;
}
static inline void M_RRA() {
	byte i = R.AF.B.l&C_FLAG;
	R.AF.B.l = (R.AF.B.l&0xEC)|(R.AF.B.h&0x01);
	R.AF.B.h = (R.AF.B.h>>1)|(i<<7);
}

static inline void M_RLC(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|q;
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_RRC(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(q<<7);
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_RL(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|(R.AF.B.l&1);
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_RR(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(R.AF.B.l<<7);
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_SLL(byte &Reg) {
	byte q = Reg>>7;
	Reg = (Reg<<1)|1;
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_SLA(byte &Reg) {
	byte q = Reg>>7;
	Reg <<= 1;
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_SRL(byte &Reg) {
	byte q = Reg&1;
	Reg >>= 1;
	R.AF.B.l = ZSPTable[Reg]|q;
}
static inline void M_SRA(byte &Reg) {
	byte q = Reg&1;
	Reg = (Reg>>1)|(Reg&0x80);
	R.AF.B.l = ZSPTable[Reg]|q;
}

static inline void M_INC(byte &Reg) {
	++Reg;
	R.AF.B.l = (R.AF.B.l&C_FLAG)|ZSTable[Reg]|
	           ((Reg==0x80)?V_FLAG:0)|((Reg&0x0F)?0:H_FLAG);
}
static inline void M_DEC(byte &Reg) {
	R.AF.B.l = (R.AF.B.l&C_FLAG)|N_FLAG|
	           ((Reg==0x80)?V_FLAG:0)|((Reg&0x0F)?0:H_FLAG);
	R.AF.B.l |= ZSTable[--Reg];
}

static inline void M_ADD(byte Reg) {
	int q = R.AF.B.h+Reg;
	R.AF.B.l = ZSTable[q&255]|((q&256)>>8)|
	           ((R.AF.B.h^q^Reg)&H_FLAG)|
	           (((Reg^R.AF.B.h^0x80)&(Reg^q)&0x80)>>5);
	R.AF.B.h = q;
}
static inline void M_ADC(byte Reg) {
	int q = R.AF.B.h+Reg+(R.AF.B.l&1);
	R.AF.B.l = ZSTable[q&255]|((q&256)>>8)|
	           ((R.AF.B.h^q^Reg)&H_FLAG)|
	           (((Reg^R.AF.B.h^0x80)&(Reg^q)&0x80)>>5);
	R.AF.B.h = q;
}
static inline void M_SUB(byte Reg) {
	int q = R.AF.B.h-Reg;
	R.AF.B.l = ZSTable[q&255]|((q&256)>>8)|N_FLAG|
	           ((R.AF.B.h^q^Reg)&H_FLAG)|
	           (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);
	R.AF.B.h = q;
}
static inline void M_SBC(byte Reg) {
	int q = R.AF.B.h-Reg-(R.AF.B.l&1);
	R.AF.B.l = ZSTable[q&255]|((q&256)>>8)|N_FLAG|
	           ((R.AF.B.h^q^Reg)&H_FLAG)|
	           (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);
	R.AF.B.h = q;
}
static inline void M_CP(byte Reg) {
	int q = R.AF.B.h-Reg;
	R.AF.B.l = ZSTable[q&255]|((q&256)>>8)|N_FLAG|
	           ((R.AF.B.h^q^Reg)&H_FLAG)|
	           (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);
}

static inline void M_ADDW(word &Reg1, word Reg2) {
	int q = Reg1+Reg2;
	R.AF.B.l = (R.AF.B.l&(S_FLAG|Z_FLAG|V_FLAG))|
	           (((Reg1^q^Reg2)&0x1000)>>8)|
	           ((q>>16)&1);
	Reg1 = q;
}
static inline void M_ADCW(word Reg) {
	int q = R.HL.W.l+Reg+(R.AF.W.l&1);
	R.AF.B.l = (((R.HL.W.l^q^Reg)&0x1000)>>8)|
	           ((q>>16)&1)|
	           ((q&0x8000)>>8)|
	           ((q&65535)?0:Z_FLAG)|
	           (((Reg^R.HL.W.l^0x8000)&(Reg^q)&0x8000)>>13);
	R.HL.W.l = q;
}
static inline void M_SBCW(word Reg) {
	int q = R.HL.W.l-Reg-(R.AF.W.l&1);
	R.AF.B.l = (((R.HL.W.l^q^Reg)&0x1000)>>8)|
	           ((q>>16)&1)|
	           ((q&0x8000)>>8)|
	           ((q&65535)?0:Z_FLAG)|
	           (((Reg^R.HL.W.l)&(Reg^q)&0x8000)>>13)|
	           N_FLAG;
	R.HL.W.l = q;
}
