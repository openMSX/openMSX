// $Id$

/*
TODO:
- How is 64K VRAM handled?
  VRAM size is never inspected by the command engine.
  How does a real MSX handle it?
  Mirroring of first 64K or empty memory space?
- How is extended VRAM handled?
  The current VDP implementation does not support it.
  Since it is not accessed by the renderer, it is possible allocate
  it here.
  But maybe it makes more sense to have all RAM managed by the VDP?
- Command engine should be updated by VDP if sprite enable or display
  enable changes, because those affect the execution speed.
*/

#include "VDPCmdEngine.hh"
#include "EmuTime.hh"
#include "VDP.hh"

#include <stdio.h>


// Constants:

static const int REG_SXL = 0x00; // VDP R#32: source X low
static const int REG_SXH = 0x01; // VDP R#32: source X high
static const int REG_SYL = 0x02; // VDP R#34: source Y low
static const int REG_SYH = 0x03; // VDP R#35: source Y high
static const int REG_DXL = 0x04; // VDP R#36: destination X low
static const int REG_DXH = 0x05; // VDP R#37: destination X high
static const int REG_DYL = 0x06; // VDP R#38: destination Y low
static const int REG_DYH = 0x07; // VDP R#39: destination Y high
static const int REG_NXL = 0x08; // VDP R#40: number X low
static const int REG_NXH = 0x09; // VDP R#41: number X high
static const int REG_NYL = 0x0A; // VDP R#42: number Y low
static const int REG_NYH = 0x0B; // VDP R#43: number Y high
static const int REG_COL = 0x0C; // VDP R#44: colour
static const int REG_ARG = 0x0D; // VDP R#45: argument
static const int REG_CMD = 0x0E; // VDP R#46: command

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

#define VDP_VRMP5(X, Y) (((Y&1023)<<7) + ((X&255)>>1))
#define VDP_VRMP6(X, Y) (((Y&1023)<<7) + ((X&511)>>2))
//#define VDP_VRMP7(X, Y) (((Y&511)<<8) + ((X&511)>>1))
//#define VDP_VRMP8(X, Y) (((Y&511)<<8) + (X&255))
#define VDP_VRMP7(X, Y) (((X&2)<<15) + ((Y&511)<<7) + ((X&511)>>2))
#define VDP_VRMP8(X, Y) (((X&1)<<16) + ((Y&511)<<7) + ((X&255)>>1))

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

inline int VDPCmdEngine::vramAddr(int x, int y)
{
	switch(scrMode) {
	case 0: return VDP_VRMP5(x, y);
	case 1: return VDP_VRMP6(x, y);
	case 2: return VDP_VRMP7(x, y);
	case 3: return VDP_VRMP8(x, y);
	}
}

inline byte VDPCmdEngine::point5(int sx, int sy)
{
	return ( vdp->getVRAM(VDP_VRMP5(sx, sy)) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::point6(int sx, int sy)
{
	return ( vdp->getVRAM(VDP_VRMP6(sx, sy)) >> (((~sx)&3)<<1) ) & 3;
}

inline byte VDPCmdEngine::point7(int sx, int sy)
{
	return ( vdp->getVRAM(VDP_VRMP7(sx, sy)) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::point8(int sx, int sy)
{
	return vdp->getVRAM(VDP_VRMP8(sx, sy));
}

inline byte VDPCmdEngine::point(int sx, int sy)
{
	switch (scrMode) {
	case 0: return point5(sx, sy);
	case 1: return point6(sx, sy);
	case 2: return point7(sx, sy);
	case 3: return point8(sx, sy);
	}
}

inline void VDPCmdEngine::psetLowLevel(
	int addr, byte colour, byte mask, byte op)
{
	switch (op) {
	case 0:
		vdp->setVRAM(
			addr, (vdp->getVRAM(addr) & mask) | colour, currentTime);
		break;
	case 1:
		vdp->setVRAM(addr,
			vdp->getVRAM(addr) & (colour | mask),
			currentTime);
		break;
	case 2:
		vdp->setVRAM(addr,
			vdp->getVRAM(addr) | colour,
			currentTime);
		break;
	case 3:
		vdp->setVRAM(addr,
			vdp->getVRAM(addr) ^ colour,
			currentTime);
		break;
	case 4:
		vdp->setVRAM(addr,
			(vdp->getVRAM(addr) & mask) | ~(colour | mask),
			currentTime);
		break;
	case 8:
		if (colour) vdp->setVRAM(addr,
			(vdp->getVRAM(addr) & mask) | colour,
			currentTime);
		break;
	case 9:
		if (colour) vdp->setVRAM(addr,
			vdp->getVRAM(addr) & (colour | mask),
			currentTime);
		break;
	case 10:
		if (colour) vdp->setVRAM(addr,
			vdp->getVRAM(addr) | colour,
			currentTime);
		break;
	case 11:
		if (colour) vdp->setVRAM(addr,
			vdp->getVRAM(addr) ^ colour,
			currentTime);
		break;
	case 12:
		if (colour) vdp->setVRAM(addr,
			(vdp->getVRAM(addr) & mask) | ~(colour|mask),
			currentTime);
		break;
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
	int dx, int dy, byte cl, byte op)
{
	switch (scrMode) {
	case 0: pset5(dx, dy, cl, op); break;
	case 1: pset6(dx, dy, cl, op); break;
	case 2: pset7(dx, dy, cl, op); break;
	case 3: pset8(dx, dy, cl, op); break;
	}
}

int VDPCmdEngine::getVdpTimingValue(const int *timingValues)
{
	// TODO: This makes command execution instantaneous.
	return 0;
	//return timingValues[vdp->getAccessTiming()];
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
		status|=0x10; /* Border detected */ \
		break; \
		} \
		if ((SX+=TX) & MX) { \
		status&=0xEF; /* Border not detected */ \
		break; \
		} \
	}

	switch (scrMode) {
	case 0: pre_srch point5(SX, SY) post_srch(256)
			break;
	case 1: pre_srch point6(SX, SY) post_srch(512)
			break;
	case 2: pre_srch point7(SX, SY) post_srch(512)
			break;
	case 3: pre_srch point8(SX, SY) post_srch(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		// Command execution done.
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		// Update SX in VDP registers.
		borderX = 0xFE00 | SX;
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

	if ((cmdReg[REG_ARG]&0x01)==0) {
		// X-Axis is major direction.
		switch (scrMode) {
		case 0: pre_loop pset5(DX, DY, CL, LO); post_linexmaj(256)
				break;
		case 1: pre_loop pset6(DX, DY, CL, LO); post_linexmaj(512)
				break;
		case 2: pre_loop pset7(DX, DY, CL, LO); post_linexmaj(512)
				break;
		case 3: pre_loop pset8(DX, DY, CL, LO); post_linexmaj(256)
				break;
		}
	}
	else {
		// Y-Axis is major direction.
		switch (scrMode) {
		case 0: pre_loop pset5(DX, DY, CL, LO); post_lineymaj(256)
				break;
		case 1: pre_loop pset6(DX, DY, CL, LO); post_lineymaj(512)
				break;
		case 2: pre_loop pset7(DX, DY, CL, LO); post_lineymaj(512)
				break;
		case 3: pre_loop pset8(DX, DY, CL, LO); post_lineymaj(256)
				break;
		}
	}

	if ((opsCount=cnt)>0) {
		// Command execution done.
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
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

	switch (scrMode) {
	case 0: pre_loop pset5(ADX, DY, CL, LO); post__x_y(256)
			break;
	case 1: pre_loop pset6(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 2: pre_loop pset7(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 3: pre_loop pset8(ADX, DY, CL, LO); post__x_y(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		// Command execution done.
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY)
		DY+=TY;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
		cmdReg[REG_NYL]=NY & 0xFF;
		cmdReg[REG_NYH]=(NY>>8) & 0x03;
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

	switch (scrMode) {
	case 0: pre_loop pset5(ADX, DY, point5(ASX, SY), LO); post_xxyy(256)
			break;
	case 1: pre_loop pset6(ADX, DY, point6(ASX, SY), LO); post_xxyy(512)
			break;
	case 2: pre_loop pset7(ADX, DY, point7(ASX, SY), LO); post_xxyy(512)
			break;
	case 3: pre_loop pset8(ADX, DY, point8(ASX, SY), LO); post_xxyy(256)
			break;
	}

	if ((opsCount=cnt)>0) {
		// Command execution done.
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
		SY+=TY;
		DY+=TY;
		}
		else
		if (SY==-1)
			DY+=TY;
		cmdReg[REG_NYL]=NY & 0xFF;
		cmdReg[REG_NYH]=(NY>>8) & 0x03;
		cmdReg[REG_SYL]=SY & 0xFF;
		cmdReg[REG_SYH]=(SY>>8) & 0x03;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
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
	if ((status&0x80)!=0x80) {

		cmdReg[REG_COL]=point(MMC.ASX, MMC.SY);
		opsCount-=getVdpTimingValue(LMMV_TIMING);
		status|=0x80;

		if (!--MMC.ANX || ((MMC.ASX+=MMC.TX)&MMC.MX)) {
			if (!(--MMC.NY & 1023) || (MMC.SY+=MMC.TY)==-1) {
				status&=0xFE;
				currEngine=&VDPCmdEngine::dummyEngine;
				if (!MMC.NY)
				MMC.DY+=MMC.TY;
				cmdReg[REG_NYL]=MMC.NY & 0xFF;
				cmdReg[REG_NYH]=(MMC.NY>>8) & 0x03;
				cmdReg[REG_SYL]=MMC.SY & 0xFF;
				cmdReg[REG_SYH]=(MMC.SY>>8) & 0x03;
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
	if ((status&0x80)!=0x80) {

		cmdReg[REG_COL]&=MASK[scrMode];
		pset(MMC.ADX, MMC.DY, cmdReg[REG_COL], MMC.LO);
		opsCount-=getVdpTimingValue(LMMV_TIMING);
		status|=0x80;

		if (!--MMC.ANX || ((MMC.ADX+=MMC.TX)&MMC.MX))
		if (!(--MMC.NY&1023) || (MMC.DY+=MMC.TY)==-1) {
			status&=0xFE;
			currEngine=&VDPCmdEngine::dummyEngine;
			if (!MMC.NY)
			MMC.DY+=MMC.TY;
			cmdReg[REG_NYL]=MMC.NY & 0xFF;
			cmdReg[REG_NYH]=(MMC.NY>>8) & 0x03;
			cmdReg[REG_DYL]=MMC.DY & 0xFF;
			cmdReg[REG_DYH]=(MMC.DY>>8) & 0x03;
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

	switch (scrMode) {
	case 0:
		pre_loop
		vdp->setVRAM(VDP_VRMP5(ADX, DY), CL, currentTime);
		post__x_y(256)
		break;
	case 1:
		pre_loop
		vdp->setVRAM(VDP_VRMP6(ADX, DY), CL, currentTime);
		post__x_y(512)
		break;
	case 2:
		pre_loop
		vdp->setVRAM(VDP_VRMP7(ADX, DY), CL, currentTime);
		post__x_y(512)
		break;
	case 3:
		pre_loop
		vdp->setVRAM(VDP_VRMP8(ADX, DY), CL, currentTime);
		post__x_y(256)
		break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY)
		DY+=TY;
		cmdReg[REG_NYL]=NY & 0xFF;
		cmdReg[REG_NYH]=(NY>>8) & 0x03;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
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

	switch (scrMode) {
	case 0:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP5(ADX, DY),
			vdp->getVRAM(VDP_VRMP5(ASX, SY)),
			currentTime);
		post_xxyy(256)
		break;
	case 1:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP6(ADX, DY),
			vdp->getVRAM(VDP_VRMP6(ASX, SY)),
			currentTime);
		post_xxyy(512)
		break;
	case 2:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP7(ADX, DY),
			vdp->getVRAM(VDP_VRMP7(ASX, SY)),
			currentTime);
		post_xxyy(512)
		break;
	case 3:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP8(ADX, DY),
			vdp->getVRAM(VDP_VRMP8(ASX, SY)),
			currentTime);
		post_xxyy(256)
		break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else if (SY==-1) {
			DY+=TY;
		}
		cmdReg[REG_NYL]=NY & 0xFF;
		cmdReg[REG_NYH]=(NY>>8) & 0x03;
		cmdReg[REG_SYL]=SY & 0xFF;
		cmdReg[REG_SYH]=(SY>>8) & 0x03;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
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

	switch (scrMode) {
	case 0:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP5(ADX, DY),
			vdp->getVRAM(VDP_VRMP5(ADX, SY)),
			currentTime);
		post__xyy(256)
		break;
	case 1:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP6(ADX, DY),
			vdp->getVRAM(VDP_VRMP6(ADX, SY)),
			currentTime);
		post__xyy(512)
		break;
	case 2:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP7(ADX, DY),
			vdp->getVRAM(VDP_VRMP7(ADX, SY)),
			currentTime);
		post__xyy(512)
		break;
	case 3:
		pre_loop
		vdp->setVRAM(
			VDP_VRMP8(ADX, DY),
			vdp->getVRAM(VDP_VRMP8(ADX, SY)),
			currentTime);
		post__xyy(256)
		break;
	}

	if ((opsCount=cnt)>0) {
		/* Command execution done */
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else if (SY==-1) {
			DY+=TY;
		}
		cmdReg[REG_NYL]=NY & 0xFF;
		cmdReg[REG_NYH]=(NY>>8) & 0x03;
		cmdReg[REG_SYL]=SY & 0xFF;
		cmdReg[REG_SYH]=(SY>>8) & 0x03;
		cmdReg[REG_DYL]=DY & 0xFF;
		cmdReg[REG_DYH]=(DY>>8) & 0x03;
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
	if ((status&0x80)!=0x80) {

		vdp->setVRAM(vramAddr(MMC.ADX, MMC.DY),
			cmdReg[REG_COL], currentTime);
		opsCount-=getVdpTimingValue(HMMV_TIMING);
		status|=0x80;

		if (!--MMC.ANX || ((MMC.ADX+=MMC.TX)&MMC.MX)) {
			if (!(--MMC.NY&1023) || (MMC.DY+=MMC.TY)==-1) {
				status&=0xFE;
				currEngine=&VDPCmdEngine::dummyEngine;
				if (!MMC.NY) MMC.DY+=MMC.TY;
				cmdReg[REG_NYL]=MMC.NY & 0xFF;
				cmdReg[REG_NYH]=(MMC.NY>>8) & 0x03;
				cmdReg[REG_DYL]=MMC.DY & 0xFF;
				cmdReg[REG_DYH]=(MMC.DY>>8) & 0x03;
			}
			else {
				MMC.ADX=MMC.DX;
				MMC.ANX=MMC.NX;
			}
		}
	}
}

void VDPCmdEngine::reportVdpCommand()
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
	byte cl = cmdReg[REG_COL];
	int sx = (cmdReg[REG_SXL]+((int)cmdReg[REG_SXH]<<8)) & 511;
	int sy = (cmdReg[REG_SYL]+((int)cmdReg[REG_SYH]<<8)) & 1023;
	int dx = (cmdReg[REG_DXL]+((int)cmdReg[REG_DXH]<<8)) & 511;
	int dy = (cmdReg[REG_DYL]+((int)cmdReg[REG_DYH]<<8)) & 1023;
	int nx = (cmdReg[REG_NXL]+((int)cmdReg[REG_NXH]<<8)) & 1023;
	int ny = (cmdReg[REG_NYL]+((int)cmdReg[REG_NYH]<<8)) & 1023;
	byte cm = cmdReg[REG_CMD] >> 4;
	byte lo = cmdReg[REG_CMD] & 0x0F;

	fprintf(stderr,
		"V9938: Opcode %02Xh %s-%s (%d,%d)->(%d,%d),%d [%d,%d]%s\n",
		cmdReg[REG_CMD], COMMANDS[cm], OPS[lo],
		sx,sy, dx,dy, cl, cmdReg[REG_ARG]&0x04? -nx:nx,
		cmdReg[REG_ARG]&0x08? -ny:ny,
		cmdReg[REG_ARG]&0x70? " on ExtVRAM":""
		);
}

void VDPCmdEngine::executeCommand()
{
	// V9938 ops only work in SCREENs 5-8.
	if (scrMode < 0) return;

	MMC.CM = cmdReg[REG_CMD] >> 4;
	if ((MMC.CM & 0x0C) != 0x0C && MMC.CM != 0) {
		// Dot operation: use only relevant bits of color.
		cmdReg[REG_COL]&=MASK[scrMode];
	}

	//reportVdpCommand();

	switch(cmdReg[REG_CMD] >> 4) {
	case CM_ABRT:
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		return;
	case CM_POINT:
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		cmdReg[REG_COL] = point(
			cmdReg[REG_SXL]+((int)cmdReg[REG_SXH]<<8),
			cmdReg[REG_SYL]+((int)cmdReg[REG_SYH]<<8)
			);
		return;
	case CM_PSET:
		status&=0xFE;
		currEngine=&VDPCmdEngine::dummyEngine;
		pset(
			cmdReg[REG_DXL]+((int)cmdReg[REG_DXH]<<8),
			cmdReg[REG_DYL]+((int)cmdReg[REG_DYH]<<8),
			cmdReg[REG_COL],
			cmdReg[REG_CMD]&0x0F
			);
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
		printf("V9938: Unrecognized opcode %02Xh\n", cmdReg[REG_CMD]);
		// TODO: Which engine belongs to invalid opcodes?
		//   Currently none is assigned, so previous is used.
		//   Assigning dummyEngine might make more sense.
		//   I wonder how the real V9938 does it.
		return;
	}

	// Fetch unconditional arguments.
	// TODO: Does the V9938 actually make a copy of the registers?
	//       This makes a difference when registers are written
	//       during command execution.
	MMC.SX = (cmdReg[REG_SXL]+((int)cmdReg[REG_SXH]<<8)) & 511;
	MMC.SY = (cmdReg[REG_SYL]+((int)cmdReg[REG_SYH]<<8)) & 1023;
	MMC.DX = (cmdReg[REG_DXL]+((int)cmdReg[REG_DXH]<<8)) & 511;
	MMC.DY = (cmdReg[REG_DYL]+((int)cmdReg[REG_DYH]<<8)) & 1023;
	MMC.NY = (cmdReg[REG_NYL]+((int)cmdReg[REG_NYH]<<8)) & 1023;
	MMC.TY = cmdReg[REG_ARG]&0x08? -1:1;
	MMC.MX = PPL[scrMode];
	MMC.CL = cmdReg[REG_COL];
	MMC.LO = cmdReg[REG_CMD] & 0x0F;

	// Argument depends on byte or dot operation.
	if ((MMC.CM & 0x0C) == 0x0C) {
		MMC.TX = cmdReg[REG_ARG]&0x04? -PPB[scrMode]:PPB[scrMode];
		MMC.NX = ((cmdReg[REG_NXL]+((int)cmdReg[REG_NXH]<<8)) & 1023)/PPB[scrMode];
	}
	else {
		MMC.TX = cmdReg[REG_ARG]&0x04? -1:1;
		MMC.NX = (cmdReg[REG_NXL]+((int)cmdReg[REG_NXH]<<8)) & 1023;
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
		? (cmdReg[REG_ARG]&0x02)!=0 // TODO: Do we look for "==" or "!="?
		: MMC.NX
		);

	// Command execution started.
	status |= 0x01;

	return;
}

// Added routines for openMSX:

VDPCmdEngine::VDPCmdEngine(VDP *vdp, const EmuTime &time)
{
	this->vdp = vdp;
	currentTime = time;

	opsCount = 1;
	currEngine = &VDPCmdEngine::dummyEngine;
	for (int i = 0; i < 15; i++) {
		cmdReg[i] = 0;
	}
	status = 0;
	borderX = 0;

	updateDisplayMode(vdp->getDisplayMode(), currentTime);
}

void VDPCmdEngine::updateDisplayMode(int mode, const EmuTime &time)
{
	sync(time);
	switch (mode) {
	case 0x0C: // SCREEN5
		scrMode = 0;
		break;
	case 0x10: // SCREEN6
		scrMode = 1;
		break;
	case 0x14: // SCREEN7
		scrMode = 2;
		break;
	case 0x1C: // SCREEN8
		scrMode = 3;
		break;
	default:
		scrMode = -1; // no commands
		break;
	}
}

