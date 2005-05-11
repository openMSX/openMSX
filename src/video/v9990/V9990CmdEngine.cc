// $Id$

#include "openmsx.hh"
#include "V9990.hh"
#include "V9990CmdEngine.hh"
#include "V9990VRAM.hh"
#include <iostream>

namespace openmsx {

static const byte DIY = 0x08;
static const byte DIX = 0x04;
static const byte NEQ = 0x02;
static const byte MAJ = 0x01;

// 2 bpp --------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp2::addressOf(int x, int y, int imageWidth)
{
	return ((x/PIXELS_PER_BYTE) % imageWidth) + y * imageWidth;
}

inline word V9990CmdEngine::V9990Bpp2::shiftDown(word value, int x)
{
	int  shift = (~x & 7) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline word V9990CmdEngine::V9990Bpp2::point(V9990VRAM* vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline void V9990CmdEngine::V9990Bpp2::pset(V9990VRAM* vram,
                                            int x, int y, int imageWidth,
                                            word color)
{
	uint addr = addressOf(x, y, imageWidth);
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	word value = (word)(vram->readVRAM(addr));
	vram->writeVRAM(addr,
		(byte)(value & ~(MASK << shift)) | ((color & MASK) << shift));
}

// 4 bpp --------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp4::addressOf(int x, int y, int imageWidth)
{
	return ((x/PIXELS_PER_BYTE) % imageWidth) + y * imageWidth;
}

inline word V9990CmdEngine::V9990Bpp4::shiftDown(word value, int x)
{
	int  shift = (~x & 3) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline word V9990CmdEngine::V9990Bpp4::point(V9990VRAM* vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline void V9990CmdEngine::V9990Bpp4::pset(V9990VRAM* vram,
                                            int x, int y, int imageWidth,
                                            word color)
{
	uint addr  = addressOf(x, y, imageWidth);
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	word value = (word)(vram->readVRAM(addr));
	vram->writeVRAM(addr,
		(byte)(value & ~(MASK << shift)) | ((color & MASK) << shift));
}

// 8 bpp --------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp8::addressOf(int x, int y, int imageWidth)
{
	return ((x/PIXELS_PER_BYTE) % imageWidth) + y * imageWidth;
}

inline word V9990CmdEngine::V9990Bpp8::shiftDown(word value, int x)
{
	return (x & 1) ? value >> 8 : value & 0xFF;
}

inline word V9990CmdEngine::V9990Bpp8::point(V9990VRAM* vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	return (value);
}

inline void V9990CmdEngine::V9990Bpp8::pset(V9990VRAM* vram,
                                            int x, int y, int imageWidth,
                                            word color)
{
	uint addr = addressOf(x, y, imageWidth);
	vram->writeVRAM(addr, (byte)(color & 0xFF));
}

// 16 bpp -------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp16::addressOf(int x, int y, int imageWidth)
{
	return ((x % imageWidth) + y * imageWidth) << 1;
}

inline word V9990CmdEngine::V9990Bpp16::shiftDown(word value, int /*x*/)
{
	return value;
}

inline word V9990CmdEngine::V9990Bpp16::point(V9990VRAM* vram,
                                             int x, int y, int imageWidth)
{
	int addr = addressOf(x,y, imageWidth);
	word value = vram->readVRAM(addr)+(vram->readVRAM(addr+1) << 8);
	return value;
}

inline void V9990CmdEngine::V9990Bpp16::pset(V9990VRAM* vram,
                                            int x, int y, int imageWidth,
                                            word color)
{
	int addr = addressOf(x,y, imageWidth);
	vram->writeVRAM(addr,   (color & 0xFF));
	vram->writeVRAM(addr+1, ((color >> 8) & 0xFF));
}

// ====================================================================
/** Constructor
  */
V9990CmdEngine::V9990CmdEngine(V9990* vdp_, const EmuTime& time)
	: vdp(vdp_)
	, cmdTraceSetting("v9990cmdtrace", "V9990 command tracing on/off", false)
{
	V9990CmdEngine::CmdSTOP* stopCmd =
		new V9990CmdEngine::CmdSTOP(this, vdp->getVRAM());
	for (int mode = 0; mode <= BP2; ++mode) {
		commands[0][mode] = stopCmd;
	}

	createEngines<CmdLMMC> (0x01);
	createEngines<CmdLMMV> (0x02);
	createEngines<CmdLMCM> (0x03);
	createEngines<CmdLMMM> (0x04);
	createEngines<CmdCMMC> (0x05);
	createEngines<CmdCMMK> (0x06);
	createEngines<CmdCMMM> (0x07);
	createEngines<CmdBMXL> (0x08);
	createEngines<CmdBMLX> (0x09);
	createEngines<CmdBMLL> (0x0A);
	createEngines<CmdLINE> (0x0B);
	createEngines<CmdSRCH> (0x0C);
	createEngines<CmdPOINT>(0x0D);
	createEngines<CmdPSET> (0x0E);
	createEngines<CmdADVN> (0x0F);

	reset(time);
}

V9990CmdEngine::~V9990CmdEngine()
{
	delete commands[0][0]; // Delete the STOP cmd
	
	for (int cmd = 1; cmd < 16; ++cmd) { // Delete the rest
		for (int mode = 0; mode <= BP2; ++mode) {
			delete commands[cmd][mode];
		}
	}
}

void V9990CmdEngine::reset(const EmuTime& /*time*/)
{
	currentCommand = NULL;
	status = 0;
	borderX = 0;
}

void V9990CmdEngine::setCmdReg(byte reg, byte value, const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] V9990CmdEngine::setCmdReg(" 
	          << std::dec << (int) reg << "," << (int) value << ")");
	sync(time);
	switch(reg - 32) {
	case  0: // SX low
		SX = (SX & 0x0700) | value;
		break;
	case  1: // SX high
		SX = (SX & 0x00FF) | ((value & 0x07) << 8);
		break;
	case  2: // SY low
		SY = (SY & 0x0F00) | value;
		break;
	case  3: // SY high
		SY = (SY & 0x00FF) | ((value & 0x0F) << 8);
		break;
	case  4: // DX low
		DX = (DX & 0x0700) | value;
		break;
	case  5: // DX high
		DX = (DX & 0x00FF) | ((value & 0x07) << 8);
		break;
	case  6: // DY low
		DY = (DY & 0x0F00) | value;
		break;
	case  7: // DY high
		DY = (DY & 0x00FF) | ((value & 0x0F) << 8);
		break;
	case  8: // NX low
		NX = (NX & 0x0F00) | value;
		break;
	case  9: // NX high
		NX = (NX & 0x00FF) | ((value & 0x0F) << 8);
		break;
	case 10: // NY low
		NY = (NY & 0x0F00) | value;
		break;
	case 11: // NY high
		NY = (NY & 0x00FF) | ((value & 0x0F) << 8);
		break;
	case 12: // ARG
		ARG = value & 0x0F;
		break;
	case 13: // LOGOP
		LOG = value & 0x1F;
		break;
	case 14: // write mask low
		WM = (WM & 0xFF00) | value;
		break;
	case 15: // write mask high
		WM = (WM & 0x00FF) | (value << 8);
		break;
	case 16: // Font color - FG low
		fgCol = (fgCol & 0xFF00) | value;
		break;
	case 17: // Font color - FG high
		fgCol = (fgCol & 0x00FF) | (value << 8);
		break;
	case 18: // Font color - BG low
		bgCol = (bgCol & 0xFF00) | value;
		break;
	case 19: // Font color - BG high
		bgCol = (bgCol & 0x00FF) | (value << 8);
		break;
	case 20: // CMD
	{
		CMD = value;
		if (currentCommand) {
			// Do Something to stop the running command
		}
		if (cmdTraceSetting.getValue()) {
			reportV9990Command();
		}
		status |= CE;
		currentCommand = commands[CMD >> 4][vdp->getColorMode()];
		if (currentCommand) currentCommand->start(time);
		break;
	}
	default: /* nada */
		;
	}
}

void V9990CmdEngine::reportV9990Command()
{
	const char* const COMMANDS[16] = {
		"STOP", "LMMC", "LMMV", "LMCM",
		"LMMM", "CMMC", "CMMK", "CMMM",
		"BMXL", "BMLX", "BMLL", "LINE",
		"SRCH", "POINT","PSET", "ADVN"
	};
	std::cerr << "V9990Cmd " << COMMANDS[CMD >> 4]
	          << " SX="  << std::dec << SX
	          << " SY="  << std::dec << SY
	          << " DX="  << std::dec << DX
	          << " DY="  << std::dec << DY
	          << " NX="  << std::dec << NX
	          << " NY="  << std::dec << NY
	          << " ARG=" << std::hex << (int)ARG
	          << " LOG=" << std::hex << (int)LOG
	          << " WM="  << std::hex << WM
	          << " FC="  << std::hex << fgCol
	          << " BC="  << std::hex << bgCol
	          << " CMD=" << std::hex << (int)CMD
	          << std::endl;
}

template <template <class Mode> class Command>
void V9990CmdEngine::createEngines(int cmd)
{
	#define CREATE_COMMAND(COLORMODE,BPP) \
		commands[cmd][COLORMODE] = \
			new Command<V9990CmdEngine::BPP>(this, vdp->getVRAM())
	
	CREATE_COMMAND(PP,    V9990Bpp4);
	CREATE_COMMAND(BYUV,  V9990Bpp8);
	CREATE_COMMAND(BYUVP, V9990Bpp8);
	CREATE_COMMAND(BYJK,  V9990Bpp8);
	CREATE_COMMAND(BYJKP, V9990Bpp8);
	CREATE_COMMAND(BD16,  V9990Bpp16);
	CREATE_COMMAND(BD8,   V9990Bpp8);
	CREATE_COMMAND(BP6,   V9990Bpp8);
	CREATE_COMMAND(BP4,   V9990Bpp4);
	CREATE_COMMAND(BP2,   V9990Bpp2);
}

// ====================================================================
// V9990Cmd

V9990CmdEngine::V9990Cmd::V9990Cmd(V9990CmdEngine* engine_,
                                   V9990VRAM*      vram_)
{
	this->engine = engine_;
	this->vram   = vram_;
}

V9990CmdEngine::V9990Cmd::~V9990Cmd()
{
}

// ====================================================================
// STOP

V9990CmdEngine::CmdSTOP::CmdSTOP(V9990CmdEngine* engine_,
                                 V9990VRAM*      vram_)
	: V9990Cmd(engine_, vram_)
{
}

void V9990CmdEngine::CmdSTOP::start(const EmuTime& /*time*/)
{
	engine->cmdReady();
}

void V9990CmdEngine::CmdSTOP::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// LMMC

template <class Mode>
V9990CmdEngine::CmdLMMC<Mode>::CmdLMMC(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMC<Mode>::start(const EmuTime& /*time*/)
{
	if (Mode::BITS_PER_PIXEL == 16) {
		engine->dstAddress = Mode::addressOf(engine->DX,
		                                     engine->DY,
		                                     engine->vdp->getImageWidth());
	}
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	engine->status &= ~TR;
}

template <>
void V9990CmdEngine::CmdLMMC<V9990CmdEngine::V9990Bpp16>::execute(
	const EmuTime& /*time*/)
{
	if (engine->status & TR) {
		engine->status &= ~TR;
		int width = engine->vdp->getImageWidth();

		byte value = vram->readVRAM(engine->dstAddress);
		byte mask = (engine->dstAddress & 1) ? engine->WM & 0xFF
		                                     : engine->WM >> 8;
		value = engine->logOp(engine->data, value, mask);
		vram->writeVRAM(engine->dstAddress, value);
		if(! (++(engine->dstAddress) & 0x01)) {
			int dx = (engine->ARG & DIX) ? -1 : 1;
			engine->DX += dx;
			if (!(--(engine->ANX))) {
				int dy = (engine->ARG & DIY) ? -1 : 1;
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if(! (--(engine->ANY))) {
					engine->cmdReady();
				} else {
					engine->ANX = engine->NX;
				}
			}
			engine->dstAddress = V9990Bpp16::addressOf(engine->DX,
			                                           engine->DY,
			                                           width);
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdLMMC<Mode>::execute(const EmuTime& /*time*/)
{
	if (engine->status & TR) {
		engine->status &= ~TR;
		int width = engine->vdp->getImageWidth() / Mode::PIXELS_PER_BYTE;
		byte data = engine->data;
		for (int i = 0; (engine->ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			byte value = Mode::point(vram, engine->DX, engine->DY, width);
			byte mask = Mode::shiftDown(engine->WM, engine->DX);
			value = engine->logOp((data >> (8 - Mode::BITS_PER_PIXEL)),
			                      value, mask);
			Mode::pset(vram, engine->DX, engine->DY, width, value);
			
			int dx = (engine->ARG & DIX) ? -1 : 1;
			engine->DX += dx;
			if (!--(engine->ANX)) {
				int dy = (engine->ARG & DIY) ? -1 : 1;
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if (!--(engine->ANY)) {
					engine->cmdReady();
				} else {
					engine->ANX = engine->NX;
				}
			}
			data <<= Mode::BITS_PER_PIXEL;
		}
	}
}

// ====================================================================
// LMMV

template <class Mode>
V9990CmdEngine::CmdLMMV<Mode>::CmdLMMV(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::start(const EmuTime& time)
{
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::execute(const EmuTime& /*time*/)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;
	while (true) {
		word value = Mode::point(vram, engine->DX, engine->DY, width);
		word mask = Mode::shiftDown(engine->WM, engine->DX);
		value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
		                      value, mask);
		Mode::pset(vram, engine->DX, engine->DY, width, value);
		
		engine->DX += dx;
		if (!--(engine->ANX)) {
			engine->DX -= (engine->NX * dx);
			engine->DY += dy;
			if (!--(engine->ANY)) {
				engine->cmdReady();
				return;
			} else {
				engine->ANX = engine->NX;
			}
		}
	}
}

// ====================================================================
// LMCM

template <class Mode>
V9990CmdEngine::CmdLMCM<Mode>::CmdLMCM(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMCM<Mode>::start(const EmuTime& /*time*/)
{
	std::cout << "V9990: LMCM not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdLMCM<Mode>::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// LMMM

template <class Mode>
V9990CmdEngine::CmdLMMM<Mode>::CmdLMMM(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::start(const EmuTime& time)
{
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::execute(const EmuTime& /*time*/)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;
	while (true) {
		word src  = Mode::point(vram, engine->SX, engine->SY, width);
		word dest = Mode::point(vram, engine->DX, engine->DY, width);
		word mask = Mode::shiftDown(engine->WM, engine->DX);
		dest = engine->logOp(src, dest, mask);
		Mode::pset(vram, engine->DX, engine->DY, width, dest);
		
		engine->DX += dx;
		engine->SX += dx;
		if (!--(engine->ANX)) {
			engine->DX -= (engine->NX * dx);
			engine->SX -= (engine->NX * dx);
			engine->DY += dy;
			engine->SY += dy;
			if (!--(engine->ANY)) {
				engine->cmdReady();
				return;
			} else {
				engine->ANX = engine->NX;
			}
		}
	}
}

// ====================================================================
// CMMC

template <class Mode>
V9990CmdEngine::CmdCMMC<Mode>::CmdCMMC(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMC<Mode>::start(const EmuTime& /*time*/)
{
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	engine->status &= ~TR;
}

template <class Mode>
void V9990CmdEngine::CmdCMMC<Mode>::execute(const EmuTime& /*time*/)
{
	if (engine->status & TR) {
		engine->status &= ~TR;

		int width = engine->vdp->getImageWidth();
		if (Mode::PIXELS_PER_BYTE) {
			// hack to avoid "warning: division by zero"
			int ppb = Mode::PIXELS_PER_BYTE; 
			width /= ppb;
		}
		int dx = (engine->ARG & DIX) ? -1 : 1;
		int dy = (engine->ARG & DIY) ? -1 : 1;
		for (int i = 0; i < 8; ++i) {
			bool bit = engine->data & 0x80;
			engine->data <<= 1;
			
			word src = bit ? engine->fgCol : engine->bgCol;
			word dest = Mode::point(vram, engine->DX, engine->DY, width);
			word mask = Mode::shiftDown(engine->WM, engine->DX);
			dest = engine->logOp(Mode::shiftDown(src, engine->DX),
					     dest, mask);
			Mode::pset(vram, engine->DX, engine->DY, width, dest);
			
			engine->DX += dx;
			if (!--(engine->ANX)) {
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if (!--(engine->ANY)) {
					engine->cmdReady();
					return;
				} else {
					engine->ANX = engine->NX;
				}
			}
		}
	}
}

// ====================================================================
// CMMK

template <class Mode>
V9990CmdEngine::CmdCMMK<Mode>::CmdCMMK(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMK<Mode>::start(const EmuTime& /*time*/)
{
	std::cout << "V9990: CMMK not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdCMMK<Mode>::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// CMMM

template <class Mode>
V9990CmdEngine::CmdCMMM<Mode>::CmdCMMM(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::start(const EmuTime& time)
{
	engine->srcAddress = (engine->SX & 0xFF) + ((engine->SY & 0x7FF) << 8);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	engine->bitsLeft = 0;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::execute(const EmuTime& /*time*/)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;
	while (true) {
		if (!engine->bitsLeft) {
			engine->data = vram->readVRAM(engine->srcAddress++);
			engine->bitsLeft = 8;
		}
		--engine->bitsLeft;
		bool bit = engine->data & 0x80;
		engine->data <<= 1;
		
		word src = bit ? engine->fgCol : engine->bgCol;
		word dest = Mode::point(vram, engine->DX, engine->DY, width);
		word mask = Mode::shiftDown(engine->WM, engine->DX);
		dest = engine->logOp(Mode::shiftDown(src, engine->DX),
		                     dest, mask);
		Mode::pset(vram, engine->DX, engine->DY, width, dest);
		
		engine->DX += dx;
		if (!--(engine->ANX)) {
			engine->DX -= (engine->NX * dx);
			engine->DY += dy;
			if (!--(engine->ANY)) {
				engine->cmdReady();
				return;
			} else {
				engine->ANX = engine->NX;
			}
		}
	}
}

// ====================================================================
// BMXL

template <class Mode>
V9990CmdEngine::CmdBMXL<Mode>::CmdBMXL(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::start(const EmuTime& time)
{
	engine->srcAddress = (engine->SX & 0xFF) + ((engine->SY & 0x7FF) << 8);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	
	// TODO should be done by sync
	execute(time);
}

template <>
void V9990CmdEngine::CmdBMXL<V9990CmdEngine::V9990Bpp16>::execute(
	const EmuTime& /*time*/)
{
	int width = engine->vdp->getImageWidth();
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;

	while (true) {
		word dest = V9990Bpp16::point(vram, engine->DX, engine->DY, width);
		word src  = vram->readVRAM(engine->srcAddress + 0) +
		            vram->readVRAM(engine->srcAddress + 1) * 256;
		word res = engine->logOp(src, dest, engine->WM);
		V9990Bpp16::pset(vram, engine->DX, engine->DY, width, res);
		engine->srcAddress += 2;
		engine->DX += dx;
		if (!(--(engine->ANX))) {
			engine->DX -= (engine->NX * dx);
			engine->DY += dy;
			if(! (--(engine->ANY))) {
				engine->cmdReady();
				return;
			} else {
				engine->ANX = engine->NX;
			}
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::execute(const EmuTime& /*time*/)
{
	int width = engine->vdp->getImageWidth() / Mode::PIXELS_PER_BYTE;
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;

	while (true) {
		byte data = vram->readVRAM(engine->srcAddress++);
		for (int i = 0; (engine->ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			word value = Mode::point(vram, engine->DX, engine->DY, width);
			word mask = Mode::shiftDown(engine->WM, engine->DX);
			value = engine->logOp((data >> (8 - Mode::BITS_PER_PIXEL)),
			                      value, mask);
			Mode::pset(vram, engine->DX, engine->DY, width, value);
			
			engine->DX += dx;
			if (!--(engine->ANX)) {
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if (!--(engine->ANY)) {
					engine->cmdReady();
					return;
				} else {
					engine->ANX = engine->NX;
				}
			}
			data <<= Mode::BITS_PER_PIXEL;
		}
	}
}

// ====================================================================
// BMLX

template <class Mode>
V9990CmdEngine::CmdBMLX<Mode>::CmdBMLX(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::start(const EmuTime& time)
{
	engine->dstAddress = (engine->DX & 0xFF) + ((engine->DY & 0x7FF) << 8);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::execute(const EmuTime& /*time*/)
{
	// TODO lots of corner cases still go wrong
	//      very dumb implementation, can be made much faster
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & DIX) ? -1 : 1;
	int dy = (engine->ARG & DIY) ? -1 : 1;

	word tmp = 0;
	engine->bitsLeft = 16;
	while (true) {
		word src  = Mode::point(vram, engine->SX, engine->SY, width);
		tmp <<= Mode::BITS_PER_PIXEL;
		tmp |= src;
		engine->bitsLeft -= Mode::BITS_PER_PIXEL;
		if (!engine->bitsLeft) {
			vram->writeVRAM(engine->dstAddress++, tmp & 0xFF);
			vram->writeVRAM(engine->dstAddress++, tmp >> 8);
			engine->bitsLeft = 16;
			tmp = 0;
		}
		
		engine->DX += dx;
		engine->SX += dx;
		if (!--(engine->ANX)) {
			engine->DX -= (engine->NX * dx);
			engine->SX -= (engine->NX * dx);
			engine->DY += dy;
			engine->SY += dy;
			if (!--(engine->ANY)) {
				engine->cmdReady();
				// TODO handle last pixels
				return;
			} else {
				engine->ANX = engine->NX;
			}
		}
	}
}

// ====================================================================
// BMLL

template <class Mode>
V9990CmdEngine::CmdBMLL<Mode>::CmdBMLL(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMLL<Mode>::start(const EmuTime& time)
{
	engine->srcAddress = (engine->SX & 0xFF) + ((engine->SY & 0x7FF) << 8);
	engine->dstAddress = (engine->DX & 0xFF) + ((engine->DY & 0x7FF) << 8);
	engine->nbBytes    = (engine->NX & 0xFF) + ((engine->NY & 0x7FF) << 8);

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdBMLL<Mode>::execute(const EmuTime& /*time*/)
{
	// TODO DIX DIY?
	//      Log op?
	while (engine->nbBytes) {
		byte src = vram->readVRAMInterleave(engine->srcAddress);
		byte dst = vram->readVRAMInterleave(engine->dstAddress);
		byte mask = (engine->dstAddress & 1)
		          ? (engine->WM >> 8) : (engine->WM & 0xFF);
		mask = 255;
		byte res = (src & mask) | (dst & ~mask); 
		vram->writeVRAMInterleave(engine->dstAddress, res);
		++engine->srcAddress;
		++engine->dstAddress;
		--engine->nbBytes;
	}
	engine->cmdReady();
}

// ====================================================================
// LINE

template <class Mode>
V9990CmdEngine::CmdLINE<Mode>::CmdLINE(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLINE<Mode>::start(const EmuTime& time)
{
	engine->ASX = (engine->NX - 1) / 2;
	engine->ADX = engine->DX;
	engine->ANX = 0;
	
	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdLINE<Mode>::execute(const EmuTime& /*time*/)
{
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}

	int TX = (engine->ARG & DIX) ? -1 : 1;
	int TY = (engine->ARG & DIY) ? -1 : 1;
	//int delta = LINE_TIMING[engine->getTiming()];

	if ((engine->ARG & MAJ) == 0) {
		// X-Axis is major direction.
		//while (clock.before(time)) {
		while (true) {
			word value = Mode::point(vram, engine->ADX, engine->DY, width);
			word mask = Mode::shiftDown(engine->WM, engine->DX);
			value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
			                      value, mask);
			Mode::pset(vram, engine->ADX, engine->DY, width, value);
			//clock += delta;
			
			engine->ADX += TX;
			if (engine->ASX < engine->NY) {
				engine->ASX += engine->NX;
				engine->DY += TY;
			}
			engine->ASX -= engine->NY;
			//engine->ASX &= 1023; // mask to 10 bits range
			if (engine->ANX++ == engine->NX || (engine->ADX & width)) {
				engine->cmdReady();
				break;
			}
		}
	} else {
		// Y-Axis is major direction.
		//while (clock.before(time)) {
		while (true) {
			word value = Mode::point(vram, engine->ADX, engine->DY, width);
			word mask = Mode::shiftDown(engine->WM, engine->DX);
			value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
			                      value, mask);
			Mode::pset(vram, engine->ADX, engine->DY, width, value);
			//clock += delta;
			engine->DY += TY;
			if (engine->ASX < engine->NY) {
				engine->ASX += engine->NX;
				engine->ADX += TX;
			}
			engine->ASX -= engine->NY;
			//engine->ASX &= 1023; // mask to 10 bits range
			if (engine->ANX++ == engine->NX || (engine->ADX & width)) {
				engine->cmdReady();
				break;
			}
		}
	}
}

// ====================================================================
// SRCH

template <class Mode>
V9990CmdEngine::CmdSRCH<Mode>::CmdSRCH(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdSRCH<Mode>::start(const EmuTime& time)
{
	engine->ASX = engine->SX;
	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdSRCH<Mode>::execute(const EmuTime& /*time*/)
{
	int ppl = engine->vdp->getImageWidth();
	int width = ppl;
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}

	word CL = Mode::shiftDown(engine->fgCol, 0);
	int TX = (engine->ARG & DIX) ? -1 : 1;
	bool AEQ = (engine->ARG & NEQ) != 0; // TODO: Do we look for "==" or "!="?
	//int delta = LINE_TIMING[engine->getTiming()];

	//while (clock.before(time)) {
	while (true) {
		//clock += delta;
		word value = Mode::point(vram, engine->ASX, engine->SY, width);
		if ((value == CL) ^ AEQ) {
			engine->status |= BD; // border detected
			engine->cmdReady();
			engine->borderX = engine->ASX;
			break;
		}
		if ((engine->ASX += TX) & ppl) {
			engine->status &= ~BD; // border not detected
			engine->cmdReady();
			engine->borderX = engine->ASX;
			break;
		}
	}
}

// ====================================================================
// POINT

template <class Mode>
V9990CmdEngine::CmdPOINT<Mode>::CmdPOINT(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdPOINT<Mode>::start(const EmuTime& /*time*/)
{
	std::cout << "V9990: POINT not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdPOINT<Mode>::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// PSET

template <class Mode>
V9990CmdEngine::CmdPSET<Mode>::CmdPSET(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdPSET<Mode>::start(const EmuTime& /*time*/)
{
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	word value = Mode::point(vram, engine->DX, engine->DY, width);
	word mask = Mode::shiftDown(engine->WM, engine->DX);
	value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
	                      value, mask);
	Mode::pset(vram, engine->DX, engine->DY, width, value);

	// TODO advance DX DY
	
	engine->cmdReady();
}

template <class Mode>
void V9990CmdEngine::CmdPSET<Mode>::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// ADVN

template <class Mode>
V9990CmdEngine::CmdADVN<Mode>::CmdADVN(V9990CmdEngine* engine,
                                       V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::start(const EmuTime& /*time*/)
{
	std::cout << "V9990: ADVN not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::execute(const EmuTime& /*time*/)
{
}

// ====================================================================
// CmdEngine methods

void V9990CmdEngine::setCmdData(byte value, const EmuTime& time)
{
	sync(time);
	data = value;
	status |= TR;
}

byte V9990CmdEngine::getCmdData(const EmuTime& time)
{
	sync(time);
	
	byte value = 0xFF;
	if (status & TR) {
		value = data;
		status &= ~TR;
	}
	return value;
}

word V9990CmdEngine::logOp(word src, word dest, word mask)
{
	if ((vdp->getDisplayMode() == P1) || (vdp->getDisplayMode() == P2)) {
		// TODO temporary workaround, ignore mask in Px modes
		//  low  byte of mask works on VRAM0
		//  high                       VRAM1
		// but interleaving is different in Bx and Px modes
		mask = 0xFFFF;
	}
	
	word value = 0;
	
	if (!((LOG & 0x10) && (src == 0))) {
		for (int i = 0; i < 16; ++i) {
			value >>= 1;
			if (mask & 1) {
				int shift = ((src & 1) << 1) | (dest & 1);
				if (LOG & (1 << shift)) {
					value |= 0x8000;
				}
			} else {
				if (dest & 1) {
					value |= 0x8000;
				}
			}
			src  >>= 1;
			dest >>= 1;
			mask >>= 1;
		}
	} else { 
		value = dest;
	}

	return value;
}

void V9990CmdEngine::cmdReady()
{
	currentCommand = NULL;
	status &= ~CE;
	vdp->cmdReady();
}

} // namespace openmsx
