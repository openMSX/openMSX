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
	commands[0x0] = abort;
	commands[0x1] = abort;
	commands[0x2] = abort;
	commands[0x3] = abort;
	commands[0x4] = new PointCmd(this, vram);
	commands[0x5] = new PsetCmd (this, vram);
	commands[0x6] = new SrchCmd (this, vram);
	commands[0x7] = new LineCmd (this, vram);
	commands[0x8] = new LmmvCmd (this, vram);
	commands[0x9] = new LmmmCmd (this, vram);
	commands[0xA] = new LmcmCmd (this, vram);
	commands[0xB] = new LmmcCmd (this, vram);
	commands[0xC] = new HmmvCmd (this, vram);
	commands[0xD] = new HmmmCmd (this, vram);
	commands[0xE] = new YmmmCmd (this, vram);
	commands[0xF] = new HmmcCmd (this, vram);

	status = 0;
	SX = SY = DX = DY = NX = NY = 0;
	COL = ARG = CMD = 0;
	LOG = OP_IMP;
}

VDPCmdEngine::~VDPCmdEngine()
{
	// skip 0, 1, 2
	for (int i = 3; i < 16; i++) {
		delete commands[i];
	}
}


void VDPCmdEngine::reset(const EmuTime &time)
{
	sync(time);
	status = 0;
	borderX = 0;
	scrMode = -1;
	for (int i = 0; i < 15; i++) {
		setCmdReg(i, 0, time);
	}

	updateDisplayMode(vdp->getDisplayMode(), time);
}

void VDPCmdEngine::setCmdReg(byte index, byte value, const EmuTime &time)
{
	sync(time);
	switch (index) {
	case 0x00: // source X low
		SX = (SX & 0x100) | value;
		break;
	case 0x01: // source X high
		SX = (SX & 0x0FF) | ((value & 0x01) << 8);
		break;
	case 0x02: // source Y low
		SY = (SY & 0x300) | value;
		break;
	case 0x03: // source Y high
		SY = (SY & 0x0FF) | ((value & 0x03) << 8);
		break;
		
	case 0x04: // destination X low
		DX = (DX & 0x100) | value;
		break;
	case 0x05: // destination X high
		DX = (DX & 0x0FF) | ((value & 0x01) << 8);
		break;
	case 0x06: // destination Y low
		DY = (DY & 0x300) | value;
		break;
	case 0x07: // destination Y high
		DY = (DY & 0x0FF) | ((value & 0x03) << 8);
		break;
	
	// TODO is DX 9 or 10 bits, at least current implementation needs
	// 10 bits (otherwise texts in UR are screwed)
	case 0x08: // number X low
		NX = (NX & 0x300) | value;
		break;
	case 0x09: // number X high
		NX = (NX & 0x0FF) | ((value & 0x03) << 8);
		break;
	case 0x0A: // number Y low
		NY = (NY & 0x300) | value;
		break;
	case 0x0B: // number Y high
		NY = (NY & 0x0FF) | ((value & 0x03) << 8);
		break;
		
	case 0x0C: // colour
		COL = value;
		status &= 0x7F;
		break;
	case 0x0D: // argument
		ARG = value;
		break;
	case 0x0E: // command
		LOG = (LogOp)(value & 0x0F);
		CMD = value >> 4;
		executeCommand(time);
		break;
	default:
		assert(false);
	}
}

void VDPCmdEngine::updateDisplayMode(DisplayMode mode, const EmuTime &time)
{
	int newScrMode;
	switch (mode.getBase()) {
	case DisplayMode::GRAPHIC4:
		newScrMode = 0;
		break;
	case DisplayMode::GRAPHIC5:
		newScrMode = 1;
		break;
	case DisplayMode::GRAPHIC6:
		newScrMode = 2;
		break;
	case DisplayMode::GRAPHIC7:
		newScrMode = 3;
		break;
	default:
		if (vdp->getCmdBit()) {
			newScrMode = 3;	// like GRAPHIC7
					// TODO timing might be different
		} else {
			newScrMode = -1;	// no commands
		}
		break;
	}

	if (newScrMode != scrMode) {
		sync(time);
		scrMode = newScrMode;
		// TODO for now abort cmd in progress, find out what really happens
		if (CMD) {
			PRT_INFO("Warning: VDP mode switch while command in progress");
			CMD = 0;
			executeCommand(time);
		}
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
	VDPCmd *cmd = commands[CMD];
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

	fprintf(stderr,
		"V9938: Opcode %s-%s (%d,%d)->(%d,%d),%d [%d,%d]%s\n",
		COMMANDS[CMD], OPS[LOG], SX, SY, DX, DY, COL,
		((ARG & 0x04) ? -NX : NX),
		((ARG & 0x08) ? -NY : NY),
		((ARG & 0x70) ? " on ExtVRAM" : ""));
}


// Inline methods first, to make sure they are actually inlined.

#define VDP_VRMP5(X, Y) (((Y & 1023) << 7) + ((X & 255) >> 1))
#define VDP_VRMP6(X, Y) (((Y & 1023) << 7) + ((X & 511) >> 2))
#define VDP_VRMP7(X, Y) (((X & 2) << 15) + ((Y & 511) << 7) + ((X & 511) >> 2))
#define VDP_VRMP8(X, Y) (((X & 1) << 16) + ((Y & 511) << 7) + ((X & 255) >> 1))

inline int VDPCmdEngine::VDPCmd::vramAddr(int x, int y)
{
	switch(engine->scrMode) {
	case 0: return VDP_VRMP5(x, y);
	case 1: return VDP_VRMP6(x, y);
	case 2: return VDP_VRMP7(x, y);
	case 3: return VDP_VRMP8(x, y);
	default: assert(false); return 0; // avoid warning
	}
}

inline byte VDPCmdEngine::VDPCmd::point5(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP5(sx, sy)) >>
	       (((~sx) & 1) << 2)) & 15;
}

inline byte VDPCmdEngine::VDPCmd::point6(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP6(sx, sy)) >>
	       (((~sx) & 3) << 1)) & 3;
}

inline byte VDPCmdEngine::VDPCmd::point7(int sx, int sy)
{
	return (vram->cmdReadWindow.readNP(VDP_VRMP7(sx, sy)) >>
	       (((~sx) & 1) << 2)) & 15;
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
	psetLowLevel(VDP_VRMP5(dx, dy), cl << sh, ~(15 << sh), op);
}

inline void VDPCmdEngine::VDPCmd::pset6(int dx, int dy, byte cl, LogOp op)
{
	byte sh = ((~dx) & 3) << 1;
	psetLowLevel(VDP_VRMP6(dx, dy), cl << sh, ~(3 << sh), op);
}

inline void VDPCmdEngine::VDPCmd::pset7(int dx, int dy, byte cl, LogOp op)
{
	byte sh = ((~dx) & 1) << 2;
	psetLowLevel(VDP_VRMP7(dx, dy), cl << sh, ~(15 << sh), op);
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
	engine->CMD = 0;
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
		if (!--ANX || ((ADX += TX) & MX)) { \
			if (!(--NY & 1023) || (DY += TY) == -1) { \
				finished = true; \
				break; \
			} else { \
				ADX = DX; \
				ANX = NX; \
			} \
		} \
	}

// Loop over DX, SY, DY.
#define post__xyy(MX) \
		if ((ADX += TX) & MX) { \
			if (!(--NY & 1023) || (SY += TY) == -1 || (DY += TY) == -1) { \
				finished = true; \
				break; \
			} else { \
				ADX = DX; \
			} \
		} \
	}

// Loop over SX, DX, SY, DY.
#define post_xxyy(MX) \
		if (!--ANX || ((ASX += TX) & MX) || ((ADX += TX) & MX)) { \
			if (!(--NY & 1023) || (SY += TY) == -1 || (DY += TY) == -1) { \
				finished = true; \
				break; \
			} else { \
				ASX = SX; \
				ADX = DX; \
				ANX = NX; \
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
}


// POINT

void VDPCmdEngine::PointCmd::start(const EmuTime &time)
{
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	
	engine->COL = point(engine->SX, engine->SY);
	commandDone();
}

void VDPCmdEngine::PointCmd::execute(const EmuTime &time)
{
}


// PSET

void VDPCmdEngine::PsetCmd::start(const EmuTime &time)
{
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	
	byte col = engine->COL & MASK[engine->scrMode];
	pset(engine->DX, engine->DY, col, engine->LOG);
	commandDone();
}

void VDPCmdEngine::PsetCmd::execute(const EmuTime &time)
{
}


// SRCH

void VDPCmdEngine::SrchCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	SX = engine->SX;
	SY = engine->SY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	CL = engine->COL & MASK[engine->scrMode];
	ANX = (engine->ARG & 0x02) != 0; // TODO: Do we look for "==" or "!="?
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
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	TY = (engine->ARG & 0x08) ? -1 : 1;
	CL = engine->COL & MASK[engine->scrMode];
	LO = engine->LOG;
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

	if ((engine->ARG & 0x01) == 0) {
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
		engine->DY = DY;
	}
}


// LMMV

void VDPCmdEngine::LmmvCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	TY = (engine->ARG & 0x08) ? -1 : 1;
	CL = engine->COL & MASK[engine->scrMode];
	LO = engine->LOG;
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
	case 0: pre_loop
		pset5(ADX, DY, CL, LO);
		post__x_y(256)
		break;
	case 1: pre_loop
		pset6(ADX, DY, CL, LO);
		post__x_y(512)
		break;
	case 2: pre_loop
		pset7(ADX, DY, CL, LO);
		post__x_y(512)
		break;
	case 3: pre_loop
		pset8(ADX, DY, CL, LO);
		post__x_y(256)
		break;
	}

	if (finished) {
		// Command execution done.
		commandDone();
		if (!NY) DY += TY;
		engine->DY = DY;
		engine->NY = NY;
	}
}


// LMMM

void VDPCmdEngine::LmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SX = engine->SX;
	SY = engine->SY;
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	TY = (engine->ARG & 0x08) ? -1 : 1;
	LO = engine->LOG;
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
	case 0: pre_loop
	        pset5(ADX, DY, point5(ASX, SY), LO);
		post_xxyy(256)
		break;
	case 1: pre_loop
		pset6(ADX, DY, point6(ASX, SY), LO);
		post_xxyy(512)
		break;
	case 2: pre_loop
		pset7(ADX, DY, point7(ASX, SY), LO);
		post_xxyy(512)
		break;
	case 3: pre_loop
		pset8(ADX, DY, point8(ASX, SY), LO);
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
		engine->NY = NY;
		engine->SY = SY;
		engine->DY = DY;
	}
}


// LMCM

void VDPCmdEngine::LmcmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.disable(currentTime);
	SX = engine->SX;
	SY = engine->SY;
	DY = engine->DY;
	NX = engine->NX;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	TY = (engine->ARG & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	ASX = SX;
	ANX = NX;
}

void VDPCmdEngine::LmcmCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		engine->COL = point(ASX, SY);
		opsCount -= getVdpTimingValue(LMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ASX += TX) & MX)) {
			if (!(--NY & 1023) || (SY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				engine->NY = NY;
				engine->SY = SY;
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
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -1 : 1;
	TY = (engine->ARG & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	LO = engine->LOG;
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::LmmcCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		byte col = engine->COL & MASK[engine->scrMode];
		pset(ADX, DY, col, LO);
		opsCount -= getVdpTimingValue(LMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ADX += TX) & MX)) {
			if (!(--NY & 1023) || (DY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				engine->NY = NY;
				engine->DY = DY;
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
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX / PPB[engine->scrMode];
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (engine->ARG & 0x08) ? -1 : 1;
	CL = engine->COL;
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
		engine->NY = NY;
		engine->DY = DY;
	}
}


// HMMM

void VDPCmdEngine::HmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SX = engine->SX;
	SY = engine->SY;
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX / PPB[engine->scrMode];
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (engine->ARG & 0x08) ? -1 : 1;
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
		engine->NY = NY;
		engine->SY = SY;
		engine->DY = DY;
	}
}


// YMMM

void VDPCmdEngine::YmmmCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	SY = engine->SY;
	DX = engine->DX;
	DY = engine->DY;
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (engine->ARG & 0x08) ? -1 : 1;
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
		engine->NY = NY;
		engine->SY = SY;
		engine->DY = DY;
	}
}


// HMMC

void VDPCmdEngine::HmmcCmd::start(const EmuTime &time)
{
	opsCount = 0;
	currentTime = time;
	vram->cmdReadWindow.disable(currentTime);
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, currentTime);
	DX = engine->DX;
	DY = engine->DY;
	NX = engine->NX / PPB[engine->scrMode];
	NY = engine->NY;
	TX = (engine->ARG & 0x04) ? -PPB[engine->scrMode] : PPB[engine->scrMode];
	TY = (engine->ARG & 0x08) ? -1 : 1;
	MX = PPL[engine->scrMode];
	ADX = DX;
	ANX = NX;
}

void VDPCmdEngine::HmmcCmd::execute(const EmuTime &time)
{
	opsCount += currentTime.getTicksTill(time);
	currentTime = time;
	if ((engine->status & 0x80) != 0x80) {
		vram->cmdWrite(vramAddr(ADX, DY), engine->COL, currentTime);
		opsCount -= getVdpTimingValue(HMMV_TIMING);
		engine->status |= 0x80;

		if (!--ANX || ((ADX += TX) & MX)) {
			if (!(--NY & 1023) || (DY += TY) == -1) {
				// Command execution done.
				commandDone();
				if (!NY) DY += TY;
				engine->NY = NY;
				engine->DY = DY;
			} else {
				ADX = DX;
				ANX = NX;
			}
		}
	}
}
