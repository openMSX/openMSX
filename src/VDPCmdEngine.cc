// $Id$

#include "VDPCmdEngine.hh"

#include <stdio.h>

// Compile hacks, should be replaced by openMSX calls.

byte ScrMode;
byte *VRAM;
byte VDP[64];
byte VDPStatus[16];

// End of compile hacks.


// Constants:

static const byte CM_ABRT  = 0x0;
static const byte CM_POINT = 0x4;
static const byte CM_PSET  = 0x5;
static const byte CM_SRCH  = 0x6;
static const byte CM_LINE  = 0x7;
static const byte CM_LMMV  = 0x8;
static const byte CM_LMMM  = 0x9;
static const byte CM_LMCM  = 0xA;
static const byte CM_LMMC  = 0xB;
static const byte CM_HMMV  = 0xC;
static const byte CM_HMMM  = 0xD;
static const byte CM_YMMM  = 0xE;
static const byte CM_HMMC  = 0xF;

static const byte MASK[4] = { 0x0F, 0x03, 0x0F, 0xFF };
static const int  PPB[4]  = { 2,4,2,1 };
static const int  PPL[4]  = { 256,512,512,256 };

                             /*  SprOn SprOn SprOf SprOf */
                             /*  ScrOf ScrOn ScrOf ScrOn */
static const int SRCH_TIMING[8]={ 818, 1025,  818,  830,   /* ntsc */
                                  696,  854,  696,  684 }; /* pal  */
static const int LINE_TIMING[8]={ 1063, 1259, 1063, 1161,
                                  904,  1026, 904,  953 };
static const int HMMV_TIMING[8]={ 439,  549,  439,  531,
                                  366,  439,  366,  427 };
static const int LMMV_TIMING[8]={ 873,  1135, 873, 1056,
                                  732,  909,  732,  854 };
static const int YMMM_TIMING[8]={ 586,  952,  586,  610,
                                  488,  720,  488,  500 };
static const int HMMM_TIMING[8]={ 818,  1111, 818,  854,
                                  684,  879,  684,  708 };
static const int LMMM_TIMING[8]={ 1160, 1599, 1160, 1172,
                                  964,  1257, 964,  977 };

// Defines:

#define VDP_VRMP5(X, Y) (VRAM + ((Y&1023)<<7) + ((X&255)>>1))
#define VDP_VRMP6(X, Y) (VRAM + ((Y&1023)<<7) + ((X&511)>>2))
//#define VDP_VRMP7(X, Y) (VRAM + ((Y&511)<<8) + ((X&511)>>1))
//#define VDP_VRMP8(X, Y) (VRAM + ((Y&511)<<8) + (X&255))
#define VDP_VRMP7(X, Y) (VRAM + ((X&2)<<15) + ((Y&511)<<7) + ((X&511)>>2))
#define VDP_VRMP8(X, Y) (VRAM + ((X&1)<<16) + ((Y&511)<<7) + ((X&255)>>1))

// Many VDP commands are executed in some kind of loop but
// essentially, there are only a few basic loop structures
// that are re-used. We define the loop structures that are
// re-used here so that they have to be entered only once.
#define pre_loop \
	while ((cnt-=delta) > 0) {

// Loop over DX, DY.
#define post__x_y(MX) \
		if (!--ANX || ((ADX+=TX)&MX)) \
			if (!(--NY&1023) || (DY+=TY)==-1) \
				break; \
			else { \
				ADX=DX; \
				ANX=NX; \
			} \
	}

// Loop over DX, SY, DY.
#define post__xyy(MX) \
		if ((ADX+=TX)&MX) \
			if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) \
				break; \
			else \
				ADX=DX; \
	}

// Loop over SX, DX, SY, DY.
#define post_xxyy(MX) \
		if (!--ANX || ((ASX+=TX)&MX) || ((ADX+=TX)&MX)) \
			if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) \
				break; \
			else { \
				ASX=SX; \
				ADX=DX; \
				ANX=NX; \
			} \
	}

// Inline methods first, to make sure they are actually inlined.

inline byte *VDPCmdEngine::vramPtr(byte mode, int x, int y)
{
	switch(mode) {
	case 0: return VDP_VRMP5(x, y);
	case 1: return VDP_VRMP6(x, y);
	case 2: return VDP_VRMP7(x, y);
	case 3: return VDP_VRMP8(x, y);
	}
}

inline byte VDPCmdEngine::point5(int sx, int sy)
{
	return ( *VDP_VRMP5(sx, sy) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::point6(int sx, int sy)
{
	return ( *VDP_VRMP6(sx, sy) >> (((~sx)&3)<<1) ) & 3;
}

inline byte VDPCmdEngine::point7(int sx, int sy)
{
	return ( *VDP_VRMP7(sx, sy) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::point8(int sx, int sy)
{
	return *VDP_VRMP8(sx, sy);
}

inline byte VDPCmdEngine::point(byte mode, int sx, int sy)
{
	switch (mode) {
	case 0: return point5(sx, sy);
	case 1: return point6(sx, sy);
	case 2: return point7(sx, sy);
	case 3: return point8(sx, sy);
	}
}

inline void VDPCmdEngine::psetLowLevel(
	byte *p, byte cl, byte m, byte op)
{
	switch (op) {
	case 0: *p = (*p & m) | cl; break;
	case 1: *p = *p & (cl | m); break;
	case 2: *p |= cl; break;
	case 3: *p ^= cl; break;
	case 4: *p = (*p & m) | ~(cl | m); break;
	case 8: if (cl) *p = (*p & m) | cl; break;
	case 9: if (cl) *p = *p & (cl | m); break;
	case 10: if (cl) *p |= cl; break;
	case 11:  if (cl) *p ^= cl; break;
	case 12:  if (cl) *p = (*p & m) | ~(cl|m); break;
	}
}

inline void VDPCmdEngine::pset5(int dx, int dy, byte cl, byte op)
{
	byte sh = ((~dx)&1)<<2;
	psetLowLevel(VDP_VRMP5(dx, dy), cl << sh, ~(15<<sh), op);
}

inline void VDPCmdEngine::pset6(int dx, int dy, byte cl, byte op)
{
	byte sh = ((~dx)&3)<<1;
	psetLowLevel(VDP_VRMP6(dx, dy), cl << sh, ~(3<<sh), op);
}

inline void VDPCmdEngine::pset7(int dx, int dy, byte cl, byte op)
{
	byte sh = ((~dx)&1)<<2;
	psetLowLevel(VDP_VRMP7(dx, dy), cl << sh, ~(15<<sh), op);
}

inline void VDPCmdEngine::pset8(int dx, int dy, byte cl, byte op)
{
	psetLowLevel(VDP_VRMP8(dx, dy), cl, 0, op);
}

inline void VDPCmdEngine::pset(
	byte mode, int dx, int dy, byte cl, byte op)
{
	switch (mode) {
	case 0: pset5(dx, dy, cl, op); break;
	case 1: pset6(dx, dy, cl, op); break;
	case 2: pset7(dx, dy, cl, op); break;
	case 3: pset8(dx, dy, cl, op); break;
	}
}

int VDPCmdEngine::getVdpTimingValue(const int *timingValues)
{
	return timingValues[
		((VDP[1]>>6) & 1) | (VDP[8] & 2) | ((VDP[9]<<1) & 4) ];
}

void VDPCmdEngine::dummyEngine()
{
}

void VDPCmdEngine::srchEngine()
{
	int SX=MMC.SX;
	int SY=MMC.SY;
	int TX=MMC.TX;
	int ANX=MMC.ANX;
	byte CL=MMC.CL;

	int delta = getVdpTimingValue(SRCH_TIMING);
	int cnt = opsCount;

#define pre_srch \
		pre_loop \
		if ((
#define post_srch(MX) \
			==CL) ^ANX) { \
		VDPStatus[2]|=0x10; /* Border detected */ \
		break; \
		} \
		if ((SX+=TX) & MX) { \
		VDPStatus[2]&=0xEF; /* Border not detected */ \
		break; \
		} \
	}

	switch (ScrMode) {
	case 5: pre_srch point5(SX, SY) post_srch(256)
			break;
	case 6: pre_srch point6(SX, SY) post_srch(512)
			break;
	case 7: pre_srch point7(SX, SY) post_srch(512)
			break;
	case 8: pre_srch point8(SX, SY) post_srch(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		/* Update SX in VDP registers */
		VDPStatus[8]=SX&0xFF;
		VDPStatus[9]=(SX>>8)|0xFE;
	}
	else {
		MMC.SX=SX;
	}
}

void VDPCmdEngine::lineEngine()
{
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NX=MMC.NX;
	int NY=MMC.NY;
	int ASX=MMC.ASX;
	int ADX=MMC.ADX;
	byte CL=MMC.CL;
	byte LO=MMC.LO;

	int delta = getVdpTimingValue(LINE_TIMING);
	int cnt = opsCount;

#define post_linexmaj(MX) \
	DX+=TX; \
	if ((ASX-=NY)<0) { \
		ASX+=NX; \
		DY+=TY; \
	} \
	ASX&=1023; /* Mask to 10 bits range */ \
	if (ADX++==NX || (DX&MX)) \
		break; \
	}
#define post_lineymaj(MX) \
	DY+=TY; \
	if ((ASX-=NY)<0) { \
		ASX+=NX; \
		DX+=TX; \
	} \
	ASX&=1023; /* Mask to 10 bits range */ \
	if (ADX++==NX || (DX&MX)) \
		break; \
	}

	if ((VDP[45]&0x01)==0) {
		/* X-Axis is major direction */
		switch (ScrMode) {
		case 5: pre_loop pset5(DX, DY, CL, LO); post_linexmaj(256)
				break;
		case 6: pre_loop pset6(DX, DY, CL, LO); post_linexmaj(512)
				break;
		case 7: pre_loop pset7(DX, DY, CL, LO); post_linexmaj(512)
				break;
		case 8: pre_loop pset8(DX, DY, CL, LO); post_linexmaj(256)
				break;
		}
	}
	else {
		/* Y-Axis is major direction */
		switch (ScrMode) {
		case 5: pre_loop pset5(DX, DY, CL, LO); post_lineymaj(256)
				break;
		case 6: pre_loop pset6(DX, DY, CL, LO); post_lineymaj(512)
				break;
		case 7: pre_loop pset7(DX, DY, CL, LO); post_lineymaj(512)
				break;
		case 8: pre_loop pset8(DX, DY, CL, LO); post_lineymaj(256)
				break;
		}
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
	}
	else {
		MMC.DX=DX;
		MMC.DY=DY;
		MMC.ASX=ASX;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::lmmvEngine()
{
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NX=MMC.NX;
	int NY=MMC.NY;
	int ADX=MMC.ADX;
	int ANX=MMC.ANX;
	byte CL=MMC.CL;
	byte LO=MMC.LO;

	int delta = getVdpTimingValue(LMMV_TIMING);
	int cnt = opsCount;

	switch (ScrMode) {
	case 5: pre_loop pset5(ADX, DY, CL, LO); post__x_y(256)
			break;
	case 6: pre_loop pset6(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 7: pre_loop pset7(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 8: pre_loop pset8(ADX, DY, CL, LO); post__x_y(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		// Command execution done.
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY)
		DY+=TY;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
		VDP[42]=NY & 0xFF;
		VDP[43]=(NY>>8) & 0x03;
	}
	else {
		MMC.DY=DY;
		MMC.NY=NY;
		MMC.ANX=ANX;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::lmmmEngine()
{
	int SX=MMC.SX;
	int SY=MMC.SY;
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NX=MMC.NX;
	int NY=MMC.NY;
	int ASX=MMC.ASX;
	int ADX=MMC.ADX;
	int ANX=MMC.ANX;
	byte LO=MMC.LO;

	int delta = getVdpTimingValue(LMMM_TIMING);
	int cnt = opsCount;

	switch (ScrMode) {
	case 5: pre_loop pset5(ADX, DY, point5(ASX, SY), LO); post_xxyy(256)
			break;
	case 6: pre_loop pset6(ADX, DY, point6(ASX, SY), LO); post_xxyy(512)
			break;
	case 7: pre_loop pset7(ADX, DY, point7(ASX, SY), LO); post_xxyy(512)
			break;
	case 8: pre_loop pset8(ADX, DY, point8(ASX, SY), LO); post_xxyy(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
		SY+=TY;
		DY+=TY;
		}
		else
		if (SY==-1)
			DY+=TY;
		VDP[42]=NY & 0xFF;
		VDP[43]=(NY>>8) & 0x03;
		VDP[34]=SY & 0xFF;
		VDP[35]=(SY>>8) & 0x03;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
	}
	else {
		MMC.SY=SY;
		MMC.DY=DY;
		MMC.NY=NY;
		MMC.ANX=ANX;
		MMC.ASX=ASX;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::lmcmEngine()
{
	if ((VDPStatus[2]&0x80)!=0x80) {

		VDPStatus[7]=VDP[44]=point(ScrMode-5, MMC.ASX, MMC.SY);
		opsCount-=getVdpTimingValue(LMMV_TIMING);
		VDPStatus[2]|=0x80;

		if (!--MMC.ANX || ((MMC.ASX+=MMC.TX)&MMC.MX)) {
			if (!(--MMC.NY & 1023) || (MMC.SY+=MMC.TY)==-1) {
				VDPStatus[2]&=0xFE;
				currEngine=&VDPCmdEngine::dummyEngine;
				if (!MMC.NY)
				MMC.DY+=MMC.TY;
				VDP[42]=MMC.NY & 0xFF;
				VDP[43]=(MMC.NY>>8) & 0x03;
				VDP[34]=MMC.SY & 0xFF;
				VDP[35]=(MMC.SY>>8) & 0x03;
			}
			else {
				MMC.ASX=MMC.SX;
				MMC.ANX=MMC.NX;
			}
		}
	}
}

void VDPCmdEngine::lmmcEngine()
{
	if ((VDPStatus[2]&0x80)!=0x80) {
		byte SM=ScrMode-5;

		VDPStatus[7]=VDP[44]&=MASK[SM];
		pset(SM, MMC.ADX, MMC.DY, VDP[44], MMC.LO);
		opsCount-=getVdpTimingValue(LMMV_TIMING);
		VDPStatus[2]|=0x80;

		if (!--MMC.ANX || ((MMC.ADX+=MMC.TX)&MMC.MX))
		if (!(--MMC.NY&1023) || (MMC.DY+=MMC.TY)==-1) {
			VDPStatus[2]&=0xFE;
			currEngine=&VDPCmdEngine::dummyEngine;
			if (!MMC.NY)
			MMC.DY+=MMC.TY;
			VDP[42]=MMC.NY & 0xFF;
			VDP[43]=(MMC.NY>>8) & 0x03;
			VDP[38]=MMC.DY & 0xFF;
			VDP[39]=(MMC.DY>>8) & 0x03;
		}
		else {
			MMC.ADX=MMC.DX;
			MMC.ANX=MMC.NX;
		}
	}
}

void VDPCmdEngine::hmmvEngine()
{
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NX=MMC.NX;
	int NY=MMC.NY;
	int ADX=MMC.ADX;
	int ANX=MMC.ANX;
	byte CL=MMC.CL;

	int delta = getVdpTimingValue(HMMV_TIMING);
	int cnt = opsCount;

	switch (ScrMode) {
	case 5: pre_loop *VDP_VRMP5(ADX, DY) = CL; post__x_y(256)
			break;
	case 6: pre_loop *VDP_VRMP6(ADX, DY) = CL; post__x_y(512)
			break;
	case 7: pre_loop *VDP_VRMP7(ADX, DY) = CL; post__x_y(512)
			break;
	case 8: pre_loop *VDP_VRMP8(ADX, DY) = CL; post__x_y(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY)
		DY+=TY;
		VDP[42]=NY & 0xFF;
		VDP[43]=(NY>>8) & 0x03;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
	}
	else {
		MMC.DY=DY;
		MMC.NY=NY;
		MMC.ANX=ANX;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::hmmmEngine()
{
	int SX=MMC.SX;
	int SY=MMC.SY;
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NX=MMC.NX;
	int NY=MMC.NY;
	int ASX=MMC.ASX;
	int ADX=MMC.ADX;
	int ANX=MMC.ANX;

	int delta = getVdpTimingValue(HMMM_TIMING);
	int cnt = opsCount;

	switch (ScrMode) {
	case 5: pre_loop *VDP_VRMP5(ADX, DY) = *VDP_VRMP5(ASX, SY); post_xxyy(256)
			break;
	case 6: pre_loop *VDP_VRMP6(ADX, DY) = *VDP_VRMP6(ASX, SY); post_xxyy(512)
			break;
	case 7: pre_loop *VDP_VRMP7(ADX, DY) = *VDP_VRMP7(ASX, SY); post_xxyy(512)
			break;
	case 8: pre_loop *VDP_VRMP8(ADX, DY) = *VDP_VRMP8(ASX, SY); post_xxyy(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
		SY+=TY;
		DY+=TY;
		}
		else
		if (SY==-1)
			DY+=TY;
		VDP[42]=NY & 0xFF;
		VDP[43]=(NY>>8) & 0x03;
		VDP[34]=SY & 0xFF;
		VDP[35]=(SY>>8) & 0x03;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
	}
	else {
		MMC.SY=SY;
		MMC.DY=DY;
		MMC.NY=NY;
		MMC.ANX=ANX;
		MMC.ASX=ASX;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::ymmmEngine()
{
	int SY=MMC.SY;
	int DX=MMC.DX;
	int DY=MMC.DY;
	int TX=MMC.TX;
	int TY=MMC.TY;
	int NY=MMC.NY;
	int ADX=MMC.ADX;

	int delta = getVdpTimingValue(YMMM_TIMING);
	int cnt = opsCount;

	switch (ScrMode) {
	case 5: pre_loop *VDP_VRMP5(ADX, DY) = *VDP_VRMP5(ADX, SY); post__xyy(256)
			break;
	case 6: pre_loop *VDP_VRMP6(ADX, DY) = *VDP_VRMP6(ADX, SY); post__xyy(512)
			break;
	case 7: pre_loop *VDP_VRMP7(ADX, DY) = *VDP_VRMP7(ADX, SY); post__xyy(512)
			break;
	case 8: pre_loop *VDP_VRMP8(ADX, DY) = *VDP_VRMP8(ADX, SY); post__xyy(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else
		if (SY==-1) DY+=TY;
		VDP[42]=NY & 0xFF;
		VDP[43]=(NY>>8) & 0x03;
		VDP[34]=SY & 0xFF;
		VDP[35]=(SY>>8) & 0x03;
		VDP[38]=DY & 0xFF;
		VDP[39]=(DY>>8) & 0x03;
	}
	else {
		MMC.SY=SY;
		MMC.DY=DY;
		MMC.NY=NY;
		MMC.ADX=ADX;
	}
}

void VDPCmdEngine::hmmcEngine()
{
	if ((VDPStatus[2]&0x80)!=0x80) {

		*vramPtr(ScrMode-5, MMC.ADX, MMC.DY)=VDP[44];
		opsCount-=getVdpTimingValue(HMMV_TIMING);
		VDPStatus[2]|=0x80;

		if (!--MMC.ANX || ((MMC.ADX+=MMC.TX)&MMC.MX)) {
			if (!(--MMC.NY&1023) || (MMC.DY+=MMC.TY)==-1) {
				VDPStatus[2]&=0xFE;
				currEngine=&VDPCmdEngine::dummyEngine;
				if (!MMC.NY)
				MMC.DY+=MMC.TY;
				VDP[42]=MMC.NY & 0xFF;
				VDP[43]=(MMC.NY>>8) & 0x03;
				VDP[38]=MMC.DY & 0xFF;
				VDP[39]=(MMC.DY>>8) & 0x03;
			}
			else {
				MMC.ADX=MMC.DX;
				MMC.ANX=MMC.NX;
			}
		}
	}
}

void VDPCmdEngine::write(byte value)
{
	VDPStatus[2]&=0x7F;

	VDPStatus[7]=VDP[44]=value;

	if (opsCount>0) (this->*currEngine)();
}

byte VDPCmdEngine::read()
{
	VDPStatus[2]&=0x7F;

	if (opsCount>0) (this->*currEngine)();

	return VDP[44];
}

void VDPCmdEngine::reportVdpCommand(register byte op)
{
	const char *OPS[16] =
	{
		"SET ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
		"TSET","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
	};
	const char *COMMANDS[16] =
	{
		" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
		" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
	};

	// Fetch arguments.
	byte cl = VDP[44];
	int sx = (VDP[32]+((int)VDP[33]<<8)) & 511;
	int sy = (VDP[34]+((int)VDP[35]<<8)) & 1023;
	int dx = (VDP[36]+((int)VDP[37]<<8)) & 511;
	int dy = (VDP[38]+((int)VDP[39]<<8)) & 1023;
	int nx = (VDP[40]+((int)VDP[41]<<8)) & 1023;
	int ny = (VDP[42]+((int)VDP[43]<<8)) & 1023;
	byte cm = op >> 4;
	byte lo = op & 0x0F;

	printf("V9938: Opcode %02Xh %s-%s (%d,%d)->(%d,%d),%d [%d,%d]%s\n",
		op, COMMANDS[cm], OPS[lo],
		sx,sy, dx,dy, cl, VDP[45]&0x04? -nx:nx,
		VDP[45]&0x08? -ny:ny,
		VDP[45]&0x70? " on ExtVRAM":""
		);
}

void VDPCmdEngine::draw(byte op)
{
	// V9938 ops only work in SCREENs 5-8.
	if (ScrMode<5) return;

	int mode = ScrMode-5;	// Screen mode index [0..3].

	MMC.CM = op >> 4;
	if ((MMC.CM & 0x0C) != 0x0C && MMC.CM != 0) {
		// Dot operation: use only relevant bits of color.
		VDPStatus[7]=(VDP[44]&=MASK[mode]);
	}

	reportVdpCommand(op);

	switch(op >> 4) {
	case CM_ABRT:
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		return;
	case CM_POINT:
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		VDPStatus[7]=VDP[44]=
			point(mode, VDP[32]+((int)VDP[33]<<8),
				VDP[34]+((int)VDP[35]<<8));
		return;
	case CM_PSET:
		VDPStatus[2]&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		pset(mode,
			VDP[36]+((int)VDP[37]<<8),
			VDP[38]+((int)VDP[39]<<8),
			VDP[44],
			op&0x0F);
		return;
	case CM_SRCH:
		currEngine=&VDPCmdEngine::srchEngine;
		break;
	case CM_LINE:
		currEngine=&VDPCmdEngine::lineEngine;
		break;
	case CM_LMMV:
		currEngine=&VDPCmdEngine::lmmvEngine;
		break;
	case CM_LMMM:
		currEngine=&VDPCmdEngine::lmmmEngine;
		break;
	case CM_LMCM:
		currEngine=&VDPCmdEngine::lmcmEngine;
		break;
	case CM_LMMC:
		currEngine=&VDPCmdEngine::lmmcEngine;
		break;
	case CM_HMMV:
		currEngine=&VDPCmdEngine::hmmvEngine;
		break;
	case CM_HMMM:
		currEngine=&VDPCmdEngine::hmmmEngine;
		break;
	case CM_YMMM:
		currEngine=&VDPCmdEngine::ymmmEngine;
		break;
	case CM_HMMC:
		currEngine=&VDPCmdEngine::hmmcEngine;
		break;
	default:
		printf("V9938: Unrecognized opcode %02Xh\n", op);
		// TODO: Which engine belongs to invalid opcodes?
		//   Currently none is assigned, so previous is used.
		//   Assigning dummyEngine might make more sense.
		//   I wonder how the real V9938 does it.
		return;
	}

	/* Fetch unconditional arguments */
	MMC.SX = (VDP[32]+((int)VDP[33]<<8)) & 511;
	MMC.SY = (VDP[34]+((int)VDP[35]<<8)) & 1023;
	MMC.DX = (VDP[36]+((int)VDP[37]<<8)) & 511;
	MMC.DY = (VDP[38]+((int)VDP[39]<<8)) & 1023;
	MMC.NY = (VDP[42]+((int)VDP[43]<<8)) & 1023;
	MMC.TY = VDP[45]&0x08? -1:1;
	MMC.MX = PPL[mode];
	MMC.CL = VDP[44];
	MMC.LO = op & 0x0F;

	/* Argument depends on byte or dot operation */
	if ((MMC.CM & 0x0C) == 0x0C) {
		MMC.TX = VDP[45]&0x04? -PPB[mode]:PPB[mode];
		MMC.NX = ((VDP[40]+((int)VDP[41]<<8)) & 1023)/PPB[mode];
	}
	else {
		MMC.TX = VDP[45]&0x04? -1:1;
		MMC.NX = (VDP[40]+((int)VDP[41]<<8)) & 1023;
	}

	// X loop variables are treated specially for LINE command.
	if (MMC.CM == CM_LINE) {
		MMC.ASX=((MMC.NX-1)>>1);
		MMC.ADX=0;
	}
	else {
		MMC.ASX = MMC.SX;
		MMC.ADX = MMC.DX;
	}

	// NX loop variable is treated specially for SRCH command.
	MMC.ANX = (MMC.CM == CM_SRCH
		? (VDP[45]&0x02)!=0 // TODO: Do we look for "==" or "!="?
		: MMC.NX
		);

	// Command execution started.
	VDPStatus[2] |= 0x01;

	// Start execution if we still have time slices.
	if ((opsCount/*-=56250*/)>0) {
		(this->*currEngine)();
	}

	return;
}

void VDPCmdEngine::loop()
{
	if (opsCount <= 0) {
		if ((opsCount += 12500) > 0) {
			(this->*currEngine)();
		}
	}
	else {
		opsCount = 12500;
		(this->*currEngine)();
	}
}

VDPCmdEngine::VDPCmdEngine()
{
	opsCount = 1;
	currEngine = &VDPCmdEngine::dummyEngine;
}
