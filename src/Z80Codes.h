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

// If we are builkding an R800 then we need R800CPU defined
//#define R800CPU 



#define M_POP(Rg)           \
        R.Rg.W.l=M_RDSTACK(R.SP.W.l)+(M_RDSTACK((R.SP.W.l+1)&65535)<<8); \
        R.SP.W.l+=2
#define M_PUSH(Rg)          \
        R.SP.W.l-=2;        \
        M_WRSTACK(R.SP.W.l,R.Rg.W.l); \
        M_WRSTACK((R.SP.W.l+1)&65535,R.Rg.W.l>>8)

#ifdef R800CPU
#define M_CALL              \
{                           \
 int q;                     \
 q=M_RDMEM_OPCODE_WORD();   \
 M_PUSH(PC);                \
 R.PC.W.l=q;                  \
 R.ICount-=2;             \
}
#else
#define M_CALL              \
{                           \
 int q;                     \
 q=M_RDMEM_OPCODE_WORD();   \
 M_PUSH(PC);                \
 R.PC.W.l=q;                  \
 R.ICount-=7;             \
}
#endif

#ifdef R800CPU
#define M_JR                \
        R.PC.W.l+=((offset)M_RDOP_ARG(R.PC.W.l))+1; R.ICount-=1
#else
#define M_JR                \
        R.PC.W.l+=((offset)M_RDOP_ARG(R.PC.W.l))+1; R.ICount-=5
#endif

#ifdef R800CPU
#define M_RET           M_POP(PC); R.ICount-=2
#else
#define M_RET           M_POP(PC); R.ICount-=6
#endif


#define M_JP                \
        R.PC.W.l=M_RDOP_ARG(R.PC.W.l)+((M_RDOP_ARG((R.PC.W.l+1)&65535))<<8)
#define M_RST(Addr)     M_PUSH(PC); R.PC.W.l=Addr
#define M_SET(Bit,Reg)  Reg|=1<<Bit
#define M_RES(Bit,Reg)  Reg&=~(1<<Bit)
#define M_BIT(Bit,Reg)      \
        R.AF.B.l=(R.AF.B.l&C_FLAG)|H_FLAG| \
        ((Reg&(1<<Bit))? ((Bit==7)?S_FLAG:0):Z_FLAG)
#define M_AND(Reg)      R.AF.B.h&=Reg; R.AF.B.l=ZSPTable[R.AF.B.h]|H_FLAG
#define M_OR(Reg)       R.AF.B.h|=Reg; R.AF.B.l=ZSPTable[R.AF.B.h]
#define M_XOR(Reg)      R.AF.B.h^=Reg; R.AF.B.l=ZSPTable[R.AF.B.h]
#define M_IN(Reg)           \
        Reg=DoIn(R.BC.B.l,R.BC.B.h); \
        R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSPTable[Reg]

#define M_RLCA              \
 R.AF.B.h=(R.AF.B.h<<1)|((R.AF.B.h&0x80)>>7); \
 R.AF.B.l=(R.AF.B.l&0xEC)|(R.AF.B.h&C_FLAG)

#define M_RRCA              \
 R.AF.B.l=(R.AF.B.l&0xEC)|(R.AF.B.h&0x01); \
 R.AF.B.h=(R.AF.B.h>>1)|(R.AF.B.h<<7)

#define M_RLA               \
{                           \
 int i;                     \
 i=R.AF.B.l&C_FLAG;         \
 R.AF.B.l=(R.AF.B.l&0xEC)|((R.AF.B.h&0x80)>>7); \
 R.AF.B.h=(R.AF.B.h<<1)|i;  \
}

#define M_RRA               \
{                           \
 int i;                     \
 i=R.AF.B.l&C_FLAG;         \
 R.AF.B.l=(R.AF.B.l&0xEC)|(R.AF.B.h&0x01); \
 R.AF.B.h=(R.AF.B.h>>1)|(i<<7);            \
}

#define M_RLC(Reg)         \
{                          \
 int q;                    \
 q=Reg>>7;                 \
 Reg=(Reg<<1)|q;           \
 R.AF.B.l=ZSPTable[Reg]|q; \
}
#define M_RRC(Reg)         \
{                          \
 int q;                    \
 q=Reg&1;                  \
 Reg=(Reg>>1)|(q<<7);      \
 R.AF.B.l=ZSPTable[Reg]|q; \
}
#define M_RL(Reg)            \
{                            \
 int q;                      \
 q=Reg>>7;                   \
 Reg=(Reg<<1)|(R.AF.B.l&1);  \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}
#define M_RR(Reg)            \
{                            \
 int q;                      \
 q=Reg&1;                    \
 Reg=(Reg>>1)|(R.AF.B.l<<7); \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}
#define M_SLL(Reg)           \
{                            \
 int q;                      \
 q=Reg>>7;                   \
 Reg=(Reg<<1)|1;             \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}
#define M_SLA(Reg)           \
{                            \
 int q;                      \
 q=Reg>>7;                   \
 Reg<<=1;                    \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}
#define M_SRL(Reg)           \
{                            \
 int q;                      \
 q=Reg&1;                    \
 Reg>>=1;                    \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}
#define M_SRA(Reg)           \
{                            \
 int q;                      \
 q=Reg&1;                    \
 Reg=(Reg>>1)|(Reg&0x80);    \
 R.AF.B.l=ZSPTable[Reg]|q;   \
}

#define M_INC(Reg)                                      \
 ++Reg;                                                 \
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[Reg]|               \
          ((Reg==0x80)?V_FLAG:0)|((Reg&0x0F)?0:H_FLAG)

#define M_DEC(Reg)                                      \
 R.AF.B.l=(R.AF.B.l&C_FLAG)|N_FLAG|                     \
          ((Reg==0x80)?V_FLAG:0)|((Reg&0x0F)?0:H_FLAG); \
 R.AF.B.l|=ZSTable[--Reg]

#define M_ADD(Reg)                                      \
{                                                       \
 int q;                                                 \
 q=R.AF.B.h+Reg;                                        \
 R.AF.B.l=ZSTable[q&255]|((q&256)>>8)|                  \
          ((R.AF.B.h^q^Reg)&H_FLAG)|                    \
          (((Reg^R.AF.B.h^0x80)&(Reg^q)&0x80)>>5);      \
 R.AF.B.h=q;                                            \
}

#define M_ADC(Reg)                                      \
{                                                       \
 int q;                                                 \
 q=R.AF.B.h+Reg+(R.AF.B.l&1);                           \
 R.AF.B.l=ZSTable[q&255]|((q&256)>>8)|                  \
          ((R.AF.B.h^q^Reg)&H_FLAG)|                    \
          (((Reg^R.AF.B.h^0x80)&(Reg^q)&0x80)>>5);      \
 R.AF.B.h=q;                                            \
}

#define M_SUB(Reg)                                      \
{                                                       \
 int q;                                                 \
 q=R.AF.B.h-Reg;                                        \
 R.AF.B.l=ZSTable[q&255]|((q&256)>>8)|N_FLAG|           \
          ((R.AF.B.h^q^Reg)&H_FLAG)|                    \
          (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);           \
 R.AF.B.h=q;                                            \
}

#define M_SBC(Reg)                                      \
{                                                       \
 int q;                                                 \
 q=R.AF.B.h-Reg-(R.AF.B.l&1);                           \
 R.AF.B.l=ZSTable[q&255]|((q&256)>>8)|N_FLAG|           \
          ((R.AF.B.h^q^Reg)&H_FLAG)|                    \
          (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);           \
 R.AF.B.h=q;                                            \
}

#define M_CP(Reg)                                       \
{                                                       \
 int q;                                                 \
 q=R.AF.B.h-Reg;                                        \
 R.AF.B.l=ZSTable[q&255]|((q&256)>>8)|N_FLAG|           \
          ((R.AF.B.h^q^Reg)&H_FLAG)|                    \
          (((Reg^R.AF.B.h)&(Reg^q)&0x80)>>5);           \
}

#define M_ADDW(Reg1,Reg2)                              \
{                                                      \
 int q;                                                \
 q=R.Reg1.W.l+R.Reg2.W.l;                                  \
 R.AF.B.l=(R.AF.B.l&(S_FLAG|Z_FLAG|V_FLAG))|           \
          (((R.Reg1.W.l^q^R.Reg2.W.l)&0x1000)>>8)|         \
          ((q>>16)&1);                                 \
 R.Reg1.W.l=q;                                         \
}

#define M_ADCW(Reg)                                            \
{                                                              \
 int q;                                                        \
 q=R.HL.W.l+R.Reg.W.l+(R.AF.W.l&1);                                  \
 R.AF.B.l=(((R.HL.W.l^q^R.Reg.W.l)&0x1000)>>8)|                    \
          ((q>>16)&1)|                                         \
          ((q&0x8000)>>8)|                                     \
          ((q&65535)?0:Z_FLAG)|                                \
          (((R.Reg.W.l^R.HL.W.l^0x8000)&(R.Reg.W.l^q)&0x8000)>>13);  \
 R.HL.W.l=q;                                                   \
}

#define M_SBCW(Reg)                                    \
{                                                      \
 int q;                                                \
 q=R.HL.W.l-R.Reg.W.l-(R.AF.W.l&1);                          \
 R.AF.B.l=(((R.HL.W.l^q^R.Reg.W.l)&0x1000)>>8)|            \
          ((q>>16)&1)|                                 \
          ((q&0x8000)>>8)|                             \
          ((q&65535)?0:Z_FLAG)|                        \
          (((R.Reg.W.l^R.HL.W.l)&(R.Reg.W.l^q)&0x8000)>>13)| \
          N_FLAG;                                      \
 R.HL.W.l=q;                                           \
}

