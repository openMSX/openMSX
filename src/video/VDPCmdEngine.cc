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
- Currently all VRAM access is done at the start time of a series of
  updates: currentTime is not increased until the very end of the sync
  method. It should ofcourse be updated after every read and write.
  An acceptable approximation would be an update after every pixel/byte
  operation.
*/

#include <stdio.h>
#include <cassert>
#include "VDPCmdEngine.hh"
#include "EmuTime.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPSettings.hh"


// Constants:
static const byte MASK[4] = { 0x0F, 0x03, 0x0F, 0xFF };
static const int  PPB[4]  = { 2, 4, 2, 1 };
static const int  PPL[4]  = { 256, 512, 512, 256 };

//                      Sprites:    On   On   Off  Off
//                      Screen:     Off  On   Off  On
static const int SRCH_TIMING[8] = {  92, 125,  92,  92 };
static const int LINE_TIMING[8] = { 120, 147, 120, 132 };
static const int HMMV_TIMING[8] = {  49,  65,  49,  62 };
static const int LMMV_TIMING[8] = {  98, 137,  98, 124 };
static const int YMMM_TIMING[8] = {  65, 125,  65,  68 };
static const int HMMM_TIMING[8] = {  92, 136,  92,  97 };
static const int LMMM_TIMING[8] = { 129, 197, 129, 132 };



VDPCmdEngine::VDPCmdEngine(VDP *vdp_)
	: vdp(vdp_)
{
	VDPVRAM *vram = vdp->getVRAM();
	AbortCmd *abort = new AbortCmd(this, vram);
	commands[CM_ABRT]  = abort;
	commands[CM_1]     = abort;
	commands[CM_2]     = abort;
	commands[CM_3]     = abort;
	commands[CM_POINT] = new PointCmd(this, vram);
	commands[CM_PSET]  = new PsetCmd (this, vram);
	commands[CM_SRCH]  = new SrchCmd (this, vram);
	commands[CM_LINE]  = new LineCmd (this, vram);
	commands[CM_LMMV]  = new LmmvCmd (this, vram);
	commands[CM_LMMM]  = new LmmmCmd (this, vram);
	commands[CM_LMCM]  = new LmcmCmd (this, vram);
	commands[CM_LMMC]  = new LmmcCmd (this, vram);
	commands[CM_HMMV]  = new HmmvCmd (this, vram);
	commands[CM_HMMM]  = new HmmmCmd (this, vram);
	commands[CM_YMMM]  = new YmmmCmd (this, vram);
	commands[CM_HMMC]  = new HmmcCmd (this, vram);
}

VDPCmdEngine::~VDPCmdEngine()
{
	for (int i = 0; i < 16; i++) {
		delete commands[i];
	}
}


void VDPCmdEngine::reset(const EmuTime &time)
{
	sync(time);
	for (int i = 0; i < 15; i++) {
		cmdReg[i] = 0;
	}
	status = 0;
	borderX = 0;

	updateDisplayMode(vdp->getDisplayMode(), time);
}

void VDPCmdEngine::updateDisplayMode(DisplayMode mode, const EmuTime &time)
{
	sync(time);
	switch (mode.getBase()) {
	case DisplayMode::GRAPHIC4:
		scrMode = 0;
		break;
	case DisplayMode::GRAPHIC5:
		scrMode = 1;
		break;
	case DisplayMode::GRAPHIC6:
		scrMode = 2;
		break;
	case DisplayMode::GRAPHIC7:
		scrMode = 3;
		break;
	default:
		if (vdp->getCmdBit()) {
			scrMode = 3;	// like GRAPHIC7
					// TODO timing might be different
		} else {
			scrMode = -1;	// no commands
		}
		break;
	}
}

void VDPCmdEngine::executeCommand(const EmuTime &time)
{
	// V9938 ops only work in SCREEN 5-8.
	// V9958 ops work in non SCREEN 5-8 when CMD bit is set
	if (scrMode < 0) {
		return;
	}

	//reportVdpCommand();

	// start command
	status |= 0x01;
	VDPCmd *cmd = commands[(cmdReg[REG_CMD] & 0xF0) >> 4];
	cmd->start(time);

	// finish command now if instantaneous command timing is active
	if (VDPSettings::instance()->getCmdTiming()->getValue()) {
		cmd->execute(time);
	}
}

void VDPCmdEngine::reportVdpCommand()
{
	const char *OPS[16] = {
		"SET ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
		"TSET","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
	};
	const char *COMMANDS[16] = {
		" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
		" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
	};

	// Fetch arguments.
	byte cl = cmdReg[REG_COL];
	int sx = (cmdReg[REG_SXL] + (cmdReg[REG_SXH] << 8)) &  511;
	int sy = (cmdReg[REG_SYL] + (cmdReg[REG_SYH] << 8)) & 1023;
	int dx = (cmdReg[REG_DXL] + (cmdReg[REG_DXH] << 8)) &  511;
	int dy = (cmdReg[REG_DYL] + (cmdReg[REG_DYH] << 8)) & 1023;
	int nx = (cmdReg[REG_NXL] + (cmdReg[REG_NXH] << 8)) & 1023;
	int ny = (cmdReg[REG_NYL] + (cmdReg[REG_NYH] << 8)) & 1023;
	byte cm = cmdReg[REG_CMD] >> 4;
	LogOp lo = (LogOp)(cmdReg[REG_CMD] & 0x0F);

	fprintf(stderr,
		"V9938: Opcode %02Xh %s-%s (%d,%d)->(%d,%d),%d [%d,%d]%s\n",
		cmdReg[REG_CMD], COMMANDS[cm], OPS[lo],
		sx,sy, dx,dy, cl, ((cmdReg[REG_ARG] & 0x04) ? -nx : nx),
		((cmdReg[REG_ARG] & 0x08) ? -ny : ny),
		((cmdReg[REG_ARG] & 0x70) ? " on ExtVRAM" : ""));
}


// Inline methods first, to make sure they are actually inlined.

#define VDP_VRMP5(X, Y) (((Y&1023)<<7) + ((X&255)>>1))
#define VDP_VRMP6(X, Y) (((Y&1023)<<7) + ((X&511)>>2))
#define VDP_VRMP7(X, Y) (((X&2)<<15) + ((Y&511)<<7) + ((X&511)>>2))
#define VDP_VRMP8(X, Y) (((X&1)<<16) + ((Y&511)<<7) + ((X&255)>>1))

inline int VDPCmdEngine::VDPCmd::vramAddr(int x, int y)
{
	switch(engine->scrMode) {
	case 0: return VDP_VRMP5(x, y);
	case 1: return VDP_VRMP6(x, y);
	case 2: return VDP_VRMP7(x, y);
	case 3: return VDP_VRMP8(x, y);
	default: assert(false); return 0;
	}
}

inline byte VDPCmdEngine::VDPCmd::point5(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP5(sx, sy)) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::VDPCmd::point6(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP6(sx, sy)) >> (((~sx)&3)<<1) ) & 3;
}

inline byte VDPCmdEngine::VDPCmd::point7(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP7(sx, sy)) >> (((~sx)&1)<<2) ) & 15;
}

inline byte VDPCmdEngine::VDPCmd::point8(int sx, int sy)
{
	return vram->cmdReadWindow.readNP(VDP_VRMP8(sx, sy));
}

inline byte VDPCmdEngine::VDPCmd::point(int sx, int sy)
{
	switch (engine->scrMode) {
	case 0: return point5(sx, sy);
	case 1: return point6(sx, sy);
	case 2: return point7(sx, sy);
	case 3: return point8(sx, sy);
	default: assert(false); return 0; // avoid warning
	}
}

inline void VDPCmdEngine::VDPCmd::psetLowLevel(
	int addr, byte colour, byte mask, LogOp op)
{
	switch (op) {
	case OP_IMP:
		vram->cmdWrite(
			addr, (vram->cmdWriteWindow.readNP(addr) & mask) | colour,
			currentTime);
		break;
	case OP_AND:
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) & (colour | mask),
			currentTime);
		break;
	case OP_OR:
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) | colour,
			currentTime);
		break;
	case OP_XOR:
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) ^ colour,
			currentTime);
		break;
	case OP_NOT:
		vram->cmdWrite(addr,
			(vram->cmdWriteWindow.readNP(addr) & mask) | ~(colour | mask),
			currentTime);
		break;
	case OP_TIMP:
		if (colour) vram->cmdWrite(addr,
			(vram->cmdWriteWindow.readNP(addr) & mask) | colour,
			currentTime);
		break;
	case OP_TAND:
		if (colour) vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) & (colour | mask),
			currentTime);
		break;
	case OP_TOR:
		if (colour) vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) | colour,
			currentTime);
		break;
	case OP_TXOR:
		if (colour) vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) ^ colour,
			currentTime);
		break;
	case OP_TNOT:
		if (colour) vram->cmdWrite(addr,
			(vram->cmdWriteWindow.readNP(addr) & mask) | ~(colour|mask),
			currentTime);
		break;
	default:
		// undefined logical operations do nothing
		break;
	}
}

inline void VDPCmdEngine::VDPCmd::pset5(int dx, int dy, byte cl, LogOp op)
{
	byte sh = ((~dx) & 1) << 2;
	psetLowLevel(VDP_VRMP5(dx, dy), cl << sh, ~(15<<sh), op);
}

inline void VDPCmdEngine::VDPCmd::pset6(int dx, int dy, byte cl, LogOp op)
{
	byte sh = ((~dx) & 3) << 1;
	psetLowLevel(VDP_VRMP6(dx, dy), cl << sh, ~(3<<sh), op);
}

inline void VDPCmdEngine::VDPCmd::pset7(int dx, int dy, byte cl, LogOp op)
{
	byte sh = ((~dx) & 1) << 2;
	psetLowLevel(VDP_VRMP7(dx, dy), cl << sh, ~(15<<sh), op);
}

inline void VDPCmdEngine::VDPCmd::pset8(int dx, int dy, byte cl, LogOp op)
{
	psetLowLevel(VDP_VRMP8(dx, dy), cl, 0, op);
}

inline void VDPCmdEngine::VDPCmd::pset(int dx, int dy, byte cl, LogOp op)
{
	switch (engine->scrMode) {
	case 0: pset5(dx, dy, cl, op); break;
	case 1: pset6(dx, dy, cl, op); break;
	case 2: pset7(dx, dy, cl, op); break;
	case 3: pset8(dx, dy, cl, op); break;
	}
}

int VDPCmdEngine::VDPCmd::getVdpTimingValue(const int *timingValues)
{
	return VDPSettings::instance()->getCmdTiming()->getValue() 
	       ? 0
	       : timingValues[engine->vdp->getAccessTiming()];
}



void VDPCmdEngine::VDPCmd::commandDone()
{
	engine->status &= 0xFE;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.disable(currentTime);
}




// Many VDP commands are executed in some kind of loop but
// essentially, there are only a few basic loop structures
// that are re-used. We define the loop structures that are
// re-used here so that they have to be entered only once.
#define pre_loop \
	while (opsCount >= delta) { \
		opsCount -= delta;

// Loop over DX, DY.
#define post__x_y(MX) \
		if (!--ANX || ((ADX+=TX)&MX)) { \
			if (!(--NY&1023) || (DY+=TY)==-1) { \
				finished = true; \
				break; \
			} else { \
				ADX=DX; \
				ANX=NX; \
			} \
		} \
	}

// Loop over DX, SY, DY.
#define post__xyy(MX) \
		if ((ADX+=TX)&MX) { \
			if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) { \
				finished = true; \
				break; \
			} else { \
				ADX=DX; \
			} \
		} \
	}

// Loop over SX, DX, SY, DY.
#define post_xxyy(MX) \
		if (!--ANX || ((ASX+=TX)&MX) || ((ADX+=TX)&MX)) { \
			if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) { \
				finished = true; \
				break; \
			} else { \
				ASX=SX; \
				ADX=DX; \
				ANX=NX; \
			} \
		} \
	}


// ABORT

void VDPCmdEngine::AbortCmd::start(const EmuTime &time)
{
	commandDone();
}

void VDPCmdEngine::AbortCmd::execute(const EmuTime &time)
{
	assert(false);
}


// POINT

void VDPCmdEngine::PointCmd::start(const EmuTime &time)
{
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	
	cmdReg(REG_COL) = point(cmdReg(REG_SXL) + (cmdReg(REG_SXH) << 8),
	                        cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8));
	commandDone();
}

void VDPCmdEngine::PointCmd::execute(const EmuTime &time)
{
	assert(false);
}


// PSET

void VDPCmdEngine::PsetCmd::start(const EmuTime &time)
{
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	pset(cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8),
	     cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8),
	     cmdReg(REG_COL), (LogOp)(cmdReg(REG_CMD) & 0x0F));
	commandDone();
}

void VDPCmdEngine::PsetCmd::execute(const EmuTime &time)
{
	assert(false);
}


// SRCH

void VDPCmdEngine::SrchCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	SX = (cmdReg(REG_SXL) + (cmdReg(REG_SXH) << 8)) & 511;
	SY = (cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8)) & 1023;
	CL = cmdReg(REG_COL);
	ANX = (cmdReg(REG_ARG) & 0x02) != 0; // TODO: Do we look for "==" or "!="?
}

void VDPCmdEngine::SrchCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(SRCH_TIMING);
	bool finished = false;

#define pre_srch \
		pre_loop \
		if ((
#define post_srch(MX) \
		== CL) ^ ANX) { \
			engine->status |= 0x10; /* Border detected */ \
			finished = true; \
			break; \
		} \
		if ((SX += TX) & MX) { \
			engine->status &= 0xEF; /* Border not detected */ \
			finished = true; \
			break; \
		} \
	}

	switch (engine->scrMode) {
	case 0: pre_srch point5(SX, SY) post_srch(256)
		break;
	case 1: pre_srch point6(SX, SY) post_srch(512)
		break;
	case 2: pre_srch point7(SX, SY) post_srch(512)
		break;
	case 3: pre_srch point8(SX, SY) post_srch(256)
		break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		// Update SX in VDP registers.
		engine->borderX = 0xFE00 | SX;
	}
}


// LINE

void VDPCmdEngine::LineCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	CL = cmdReg(REG_COL);
	LO = (LogOp)(cmdReg(REG_CMD) & 0x0F);
	TX = (cmdReg(REG_ARG) & 0x04) ? -1 : 1;
	NX = (cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023;
	ASX = ((NX - 1) >> 1);
	ADX = 0;
}

void VDPCmdEngine::LineCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(LINE_TIMING);
	bool finished = false;

#define post_linexmaj(MX) \
		DX += TX; \
		if ((ASX -= NY) < 0) { \
			ASX += NX; \
			DY += TY; \
		} \
		ASX &= 1023; /* Mask to 10 bits range */ \
		if (ADX++ == NX || (DX & MX)) { \
			finished = true; \
			break; \
		} \
	}
#define post_lineymaj(MX) \
		DY += TY; \
		if ((ASX -= NY) < 0) { \
			ASX += NX; \
			DX += TX; \
		} \
		ASX &= 1023; /* Mask to 10 bits range */ \
		if (ADX++ == NX || (DX & MX)) { \
			finished = true; \
			break; \
		} \
	}

	if ((cmdReg(REG_ARG) & 0x01) == 0) {
		// X-Axis is major direction.
		switch (engine->scrMode) {
		case 0: pre_loop pset5(DX, DY, CL, LO); post_linexmaj(256)
			break;
		case 1: pre_loop pset6(DX, DY, CL, LO); post_linexmaj(512)
			break;
		case 2: pre_loop pset7(DX, DY, CL, LO); post_linexmaj(512)
			break;
		case 3: pre_loop pset8(DX, DY, CL, LO); post_linexmaj(256)
			break;
		}
	} else {
		// Y-Axis is major direction.
		switch (engine->scrMode) {
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

	if (finished) {
		// Command execution done.
		commandDone();
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
	}
}


// LMMV

void VDPCmdEngine::LmmvCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	CL = cmdReg(REG_COL);
	LO = (LogOp)(cmdReg(REG_CMD) & 0x0F);
	TX = (cmdReg(REG_ARG) & 0x04) ? -1 : 1;
	NX = (cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023;
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::LmmvCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(LMMV_TIMING);
	bool finished = false;

	switch (engine->scrMode) {
	case 0: pre_loop pset5(ADX, DY, CL, LO); post__x_y(256)
			break;
	case 1: pre_loop pset6(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 2: pre_loop pset7(ADX, DY, CL, LO); post__x_y(512)
			break;
	case 3: pre_loop pset8(ADX, DY, CL, LO); post__x_y(256)
			break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) DY += TY;
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
		cmdReg(REG_NYL) = NY & 0xFF;
		cmdReg(REG_NYH) = (NY >> 8) & 0x03;
	}
}


// LMMM

void VDPCmdEngine::LmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SX = (cmdReg(REG_SXL) + (cmdReg(REG_SXH) << 8)) & 511;
	SY = (cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8)) & 1023;
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	LO = (LogOp)(cmdReg(REG_CMD) & 0x0F);
	TX = (cmdReg(REG_ARG) & 0x04) ? -1 : 1;
	NX = (cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023;
	ASX = SX;
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::LmmmCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(LMMM_TIMING);
	bool finished = false;

	switch (engine->scrMode) {
	case 0: pre_loop pset5(ADX, DY, point5(ASX, SY), LO); post_xxyy(256)
			break;
	case 1: pre_loop pset6(ADX, DY, point6(ASX, SY), LO); post_xxyy(512)
			break;
	case 2: pre_loop pset7(ADX, DY, point7(ASX, SY), LO); post_xxyy(512)
			break;
	case 3: pre_loop pset8(ADX, DY, point8(ASX, SY), LO); post_xxyy(256)
			break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) {
			SY += TY;
			DY += TY;
		} else if (SY == -1) {
			DY += TY;
		}
		cmdReg(REG_NYL) = NY & 0xFF;
		cmdReg(REG_NYH) = (NY >> 8) & 0x03;
		cmdReg(REG_SYL) = SY & 0xFF;
		cmdReg(REG_SYH) = (SY >> 8) & 0x03;
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
	}
}


// LMCM

void VDPCmdEngine::LmcmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	SX = (cmdReg(REG_SXL) + (cmdReg(REG_SXH) << 8)) & 511;
	SY = (cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8)) & 1023;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NX = (cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -1 : 1;
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	ASX = SX;
	ANX = NX;
}

void VDPCmdEngine::LmcmCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		cmdReg(REG_COL) = point(ASX, SY);
		opsCount -= getVdpTimingValue(LMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ASX += TX) & MX)) {
			if (!(--NY & 1023) || (SY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				cmdReg(REG_NYL) = NY & 0xFF;
				cmdReg(REG_NYH) = (NY >> 8) & 0x03;
				cmdReg(REG_SYL) = SY & 0xFF;
				cmdReg(REG_SYH) = (SY >> 8) & 0x03;
			} else {
				ASX = SX;
				ANX = NX;
			}
		}
	}
}


// LMMC

void VDPCmdEngine::LmmcCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	cmdReg(REG_COL) &= MASK[engine->scrMode];
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NX = (cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -1 : 1;
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	LO = (LogOp)(cmdReg(REG_CMD) & 0x0F);
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::LmmcCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		cmdReg(REG_COL) &= MASK[engine->scrMode];
		pset(ADX, DY, cmdReg(REG_COL), LO);
		opsCount -= getVdpTimingValue(LMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ADX += TX) & MX)) {
			if (!(--NY & 1023) || (DY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				cmdReg(REG_NYL) = NY & 0xFF;
				cmdReg(REG_NYH) = (NY >> 8) & 0x03;
				cmdReg(REG_DYL) = DY & 0xFF;
				cmdReg(REG_DYH) = (DY >> 8) & 0x03;
			} else {
				ADX = DX;
				ANX = NX;
			}
		}
	}
}



// HMMV

void VDPCmdEngine::HmmvCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NX = ((cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023) / PPB[engine->scrMode];
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	CL = cmdReg(REG_COL);
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::HmmvCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(HMMV_TIMING);
	bool finished = false;

	switch (engine->scrMode) {
	case 0:
		pre_loop
		vram->cmdWrite(VDP_VRMP5(ADX, DY), CL, currentTime);
		post__x_y(256)
		break;
	case 1:
		pre_loop
		vram->cmdWrite(VDP_VRMP6(ADX, DY), CL, currentTime);
		post__x_y(512)
		break;
	case 2:
		pre_loop
		vram->cmdWrite(VDP_VRMP7(ADX, DY), CL, currentTime);
		post__x_y(512)
		break;
	case 3:
		pre_loop
		vram->cmdWrite(VDP_VRMP8(ADX, DY), CL, currentTime);
		post__x_y(256)
		break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) DY += TY;
		cmdReg(REG_NYL) = NY & 0xFF;
		cmdReg(REG_NYH) = (NY >> 8) & 0x03;
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
	}
}


// HMMM

void VDPCmdEngine::HmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SX = (cmdReg(REG_SXL) + (cmdReg(REG_SXH) << 8)) & 511;
	SY = (cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8)) & 1023;
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NX = ((cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023) / PPB[engine->scrMode];
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	ASX = SX;
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::HmmmCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(HMMM_TIMING);
	bool finished = false;

	switch (engine->scrMode) {
	case 0:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP5(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP5(ASX, SY)),
			currentTime);
		post_xxyy(256)
		break;
	case 1:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP6(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP6(ASX, SY)),
			currentTime);
		post_xxyy(512)
		break;
	case 2:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP7(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP7(ASX, SY)),
			currentTime);
		post_xxyy(512)
		break;
	case 3:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP8(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP8(ASX, SY)),
			currentTime);
		post_xxyy(256)
		break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) {
			SY += TY;
			DY += TY;
		} else if (SY == -1) {
			DY += TY;
		}
		cmdReg(REG_NYL) = NY & 0xFF;
		cmdReg(REG_NYH) = (NY >> 8) & 0x03;
		cmdReg(REG_SYL) = SY & 0xFF;
		cmdReg(REG_SYH) = (SY >> 8) & 0x03;
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
	}
}


// YMMM

void VDPCmdEngine::YmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SY = (cmdReg(REG_SYL) + (cmdReg(REG_SYH) << 8)) & 1023;
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	ADX = DX;
}

void VDPCmdEngine::YmmmCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	int delta = getVdpTimingValue(YMMM_TIMING);
	bool finished = false;

	switch (engine->scrMode) {
	case 0:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP5(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP5(ADX, SY)),
			currentTime);
		post__xyy(256)
		break;
	case 1:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP6(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP6(ADX, SY)),
			currentTime);
		post__xyy(512)
		break;
	case 2:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP7(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP7(ADX, SY)),
			currentTime);
		post__xyy(512)
		break;
	case 3:
		pre_loop
		vram->cmdWrite(
			VDP_VRMP8(ADX, DY),
			vram->cmdReadWindow.readNP(VDP_VRMP8(ADX, SY)),
			currentTime);
		post__xyy(256)
		break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) {
			SY += TY;
			DY += TY;
		} else if (SY == -1) {
			DY += TY;
		}
		cmdReg(REG_NYL) = NY & 0xFF;
		cmdReg(REG_NYH) = (NY >> 8) & 0x03;
		cmdReg(REG_SYL) = SY & 0xFF;
		cmdReg(REG_SYH) = (SY >> 8) & 0x03;
		cmdReg(REG_DYL) = DY & 0xFF;
		cmdReg(REG_DYH) = (DY >> 8) & 0x03;
	}
}


// HMMC

void VDPCmdEngine::HmmcCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = (cmdReg(REG_DXL) + (cmdReg(REG_DXH) << 8)) & 511;
	DY = (cmdReg(REG_DYL) + (cmdReg(REG_DYH) << 8)) & 1023;
	NX = ((cmdReg(REG_NXL) + (cmdReg(REG_NXH) << 8)) & 1023) / PPB[engine->scrMode];
	NY = (cmdReg(REG_NYL) + (cmdReg(REG_NYH) << 8)) & 1023;
	TX = (cmdReg(REG_ARG) & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (cmdReg(REG_ARG) & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::HmmcCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		vram->cmdWrite(vramAddr(ADX, DY),
			cmdReg(REG_COL), currentTime);
		opsCount -= getVdpTimingValue(HMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ADX += TX) & MX)) {
			if (!(--NY & 1023) || (DY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				cmdReg(REG_NYL) = NY & 0xFF;
				cmdReg(REG_NYH) = (NY >> 8) & 0x03;
				cmdReg(REG_DYL) = DY & 0xFF;
				cmdReg(REG_DYH) = (DY >> 8) & 0x03;
			}
		}
	}
}
