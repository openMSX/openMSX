// $Id$

#include "openmsx.hh"
#include "V9990.hh"
#include "V9990CmdEngine.hh"
#include "V9990VRAM.hh"

namespace openmsx {

// 2 bpp --------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp2::addressOf(int x, int y, int imageWidth)
{
	return ((x/PIXELS_PER_BYTE) % imageWidth) + y * imageWidth;
}

inline byte V9990CmdEngine::V9990Bpp2::writeMask(int x) {
	return (byte) (~x & ADDRESS_MASK);
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

inline byte V9990CmdEngine::V9990Bpp4::writeMask(int x) {
	return 0x00;
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

inline byte V9990CmdEngine::V9990Bpp8::writeMask(int x) {
	return 0x00;
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

inline byte V9990CmdEngine::V9990Bpp16::writeMask(int x) {
	return 0x00;
}

inline word V9990CmdEngine::V9990Bpp16::shiftDown(word value, int x)
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
V9990CmdEngine::V9990CmdEngine(V9990 *vdp_)
{
	vdp = vdp_;

	V9990CmdEngine::CmdSTOP* stopCmd =
		new V9990CmdEngine::CmdSTOP(this, vdp->getVRAM());

	commands[0][0]  = NULL;
	for (int mode = 0; mode < (BP2+1); mode++) {
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

	transfer = false;
	currentCommand = NULL;
}

V9990CmdEngine::~V9990CmdEngine()
{
	delete commands[0][0]; // Delete the STOP cmd
	
	for(int cmd = 1; cmd < 16; cmd++) { // Delete the rest
		for(int mode = 0; mode < (BP2+1); mode++) {
			if(commands[cmd][mode]) delete commands[cmd][mode];
		}
	}
}

void V9990CmdEngine::reset(const EmuTime& time)
{
	currentCommand = NULL;
	transfer = false;
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
		ARG = value &0x0F;
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
		int cmd = (value >> 4) & 0x0F;
		if (currentCommand) {
			// Do Something to stop the running command
		}
		vdp->cmdStart();
		currentCommand = commands[cmd][vdp->getColorMode()];
		if (currentCommand) currentCommand->start(time);
		break;
	}
	default: /* nada */
		;
	}
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

void V9990CmdEngine::CmdSTOP::start(const EmuTime& time)
{
	engine->cmdReady();
}

void V9990CmdEngine::CmdSTOP::execute(const EmuTime& time)
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
void V9990CmdEngine::CmdLMMC<Mode>::start(const EmuTime& time)
{
	PRT_DEBUG("LMMC: DX=" << std::dec << engine->DX <<
	          " DY=" << std::dec << engine->DY <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " WM=" << std::hex << engine->WM <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	if (Mode::BITS_PER_PIXEL == 16) {
		engine->dstAddress = Mode::addressOf(engine->DX,
		                                     engine->DY,
		                                     engine->vdp->getImageWidth());
	}
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	engine->transfer = false;
}

void V9990CmdEngine::CmdLMMC<V9990CmdEngine::V9990Bpp16>::execute(const EmuTime& time)
{
	if (engine->transfer) {
		engine->transfer = false;
		int width = engine->vdp->getImageWidth();

		byte value = vram->readVRAM(engine->dstAddress);
		value = engine->logOp(engine->data, value);
		vram->writeVRAM(engine->dstAddress, value);
		if(! (++(engine->dstAddress) & 0x01)) {
			int dx = (engine->ARG & 0x04) ? -1 : 1;
			engine->DX += dx;
			if (!(--(engine->ANX))) {
				int dy = (engine->ARG & 0x08) ? -1 : 1;
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
void V9990CmdEngine::CmdLMMC<Mode>::execute(const EmuTime& time)
{
	if (engine->transfer) {
		engine->transfer = false;
		int width = engine->vdp->getImageWidth() / Mode::PIXELS_PER_BYTE;
		byte data = engine->data;
		for (int i = 0; (engine->ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			byte value = Mode::point(vram, engine->DX, engine->DY, width);
			value = engine->logOp((data >> (8 - Mode::BITS_PER_PIXEL)),
			                      value, Mode::MASK);
			Mode::pset(vram, engine->DX, engine->DY, width, value);
			
			int dx = (engine->ARG & 0x04) ? -1 : 1;
			engine->DX += dx;
			if (!--(engine->ANX)) {
				int dy = (engine->ARG & 0x08) ? -1 : 1;
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
	PRT_DEBUG("LMMV: DX=" << std::dec << engine->DX <<
	          " DY=" << std::dec << engine->DY <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " WM=" << std::hex << engine->WM <<
	          " COL="<< std::hex << engine->fgCol <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::execute(const EmuTime& time)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;
	while (true) {
		word value = Mode::point(vram, engine->DX, engine->DY, width);
		value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
		                      value, Mode::MASK);
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
void V9990CmdEngine::CmdLMCM<Mode>::start(const EmuTime& time)
{
	std::cout << "V9990: LMCM not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdLMCM<Mode>::execute(const EmuTime& time)
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
	PRT_DEBUG("LMMM: SX=" << std::dec << engine->SX <<
	          " SY=" << std::dec << engine->SY <<
	          " DX=" << std::dec << engine->DX <<
	          " DY=" << std::dec << engine->DY <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " LOG="<< std::hex << (int)engine->LOG <<
	          " WM=" << std::hex << engine->WM <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::execute(const EmuTime& time)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;
	while (true) {
		word src  = Mode::point(vram, engine->SX, engine->SY, width);
		word dest = Mode::point(vram, engine->DX, engine->DY, width);
		dest = engine->logOp(src, dest, Mode::MASK);
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
void V9990CmdEngine::CmdCMMC<Mode>::start(const EmuTime& time)
{
	std::cout << "V9990: CMMC not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdCMMC<Mode>::execute(const EmuTime& time)
{
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
void V9990CmdEngine::CmdCMMK<Mode>::start(const EmuTime& time)
{
	std::cout << "V9990: CMMK not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdCMMK<Mode>::execute(const EmuTime& time)
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
	engine->srcAddress = (engine->SX & 0xFF) + ((engine->SY & 0x3FF) << 8);

	PRT_DEBUG("CMMM: ADR=0x" << std::hex << engine->srcAddress <<
	          " DX=" << std::dec << engine->DX <<
	          " DY=" << std::dec << engine->DY <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " WM=" << std::hex << engine->WM <<
	          " FC=" << std::hex << engine->fgCol <<
	          " BC=" << std::hex << engine->bgCol <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	engine->bitsLeft = 0;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::execute(const EmuTime& time)
{
	// TODO can be optimized a lot
	
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;
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
		dest = engine->logOp(Mode::shiftDown(src, engine->DX),
		                     dest, Mode::MASK);
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
	engine->srcAddress = (engine->SX & 0xFF) + ((engine->SY & 0x3FF) << 8);

	PRT_DEBUG("BMXL: DX=" << std::dec << engine->DX <<
	          " DY=" << std::dec << engine->DY <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " WM=" << std::hex << engine->WM <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	if (Mode::BITS_PER_PIXEL == 16) {
		engine->dstAddress = Mode::addressOf(engine->DX,
						     engine->DY,
						     engine->vdp->getImageWidth());
	}
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	
	// TODO should be done by sync
	execute(time);
}

void V9990CmdEngine::CmdBMXL<V9990CmdEngine::V9990Bpp16>::execute(const EmuTime& time)
{
	int width = engine->vdp->getImageWidth();
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;

	while (true) {
		byte value = vram->readVRAM(engine->dstAddress);
		byte data = vram->readVRAM(engine->srcAddress++);
		value = engine->logOp(data, value);
		vram->writeVRAM(engine->dstAddress, value);
		if(! (++(engine->dstAddress) & 0x01)) {
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
			engine->dstAddress = V9990Bpp16::addressOf(engine->DX,
			                                           engine->DY,
			                                           width);
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::execute(const EmuTime& time)
{
	int width = engine->vdp->getImageWidth() / Mode::PIXELS_PER_BYTE;
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;

	while (true) {
		byte data = vram->readVRAM(engine->srcAddress++);
		for (int i = 0; (engine->ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			byte value = Mode::point(vram, engine->DX, engine->DY, width);
			value = engine->logOp((data >> (8 - Mode::BITS_PER_PIXEL)),
			                      value, Mode::MASK);
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
	engine->dstAddress = (engine->DX & 0xFF) + ((engine->DY & 0x3FF) << 8);

	PRT_DEBUG("BMLX: SX=" << std::dec << engine->SX <<
	          " SY=" << std::dec << engine->SY <<
	          " ADR="<< std::hex << engine->dstAddress <<
	          " NX=" << std::dec << engine->NX <<
	          " NY=" << std::dec << engine->NY <<
	          " ARG="<< std::hex << (int)engine->ARG <<
	          " LOG="<< std::hex << (int)engine->LOG <<
	          " WM=" << std::hex << engine->WM <<
	          " Bpp="<< std::hex << Mode::PIXELS_PER_BYTE);

	engine->ANX = engine->NX;
	engine->ANY = engine->NY;

	// TODO should be done by sync
	execute(time);
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::execute(const EmuTime& time)
{
	// TODO lots of corner cases still go wrong
	//      very dumb implementation, can be made much faster
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	int dx = (engine->ARG & 0x04) ? -1 : 1;
	int dy = (engine->ARG & 0x08) ? -1 : 1;

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
	std::cout << "V9990: BMLL not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdBMLL<Mode>::execute(const EmuTime& time)
{
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
	std::cout << "V9990: LINE not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdLINE<Mode>::execute(const EmuTime& time)
{
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
	std::cout << "V9990: SRCH not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdSRCH<Mode>::execute(const EmuTime& time)
{
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
void V9990CmdEngine::CmdPOINT<Mode>::start(const EmuTime& time)
{
	std::cout << "V9990: POINT not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdPOINT<Mode>::execute(const EmuTime& time)
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
void V9990CmdEngine::CmdPSET<Mode>::start(const EmuTime& time)
{
	int width = engine->vdp->getImageWidth();
	if (Mode::PIXELS_PER_BYTE) {
		// hack to avoid "warning: division by zero"
		int ppb = Mode::PIXELS_PER_BYTE; 
		width /= ppb;
	}
	word value = Mode::point(vram, engine->DX, engine->DY, width);
	value = engine->logOp(Mode::shiftDown(engine->fgCol, engine->DX),
	                      value, Mode::MASK);
	Mode::pset(vram, engine->DX, engine->DY, width, value);

	// TODO advance DX DY
	
	engine->cmdReady();
}

template <class Mode>
void V9990CmdEngine::CmdPSET<Mode>::execute(const EmuTime& time)
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
void V9990CmdEngine::CmdADVN<Mode>::start(const EmuTime& time)
{
	std::cout << "V9990: ADVN not yet implemented" << std::endl;
	engine->cmdReady(); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::execute(const EmuTime& time)
{
}

// ====================================================================
// CmdEngine methods

void V9990CmdEngine::setCmdData(byte value, const EmuTime& time)
{
	sync(time);
	data = value;
	transfer = true;
}

byte V9990CmdEngine::getCmdData(const EmuTime& time)
{
	sync(time);
	
	byte value = 0xFF;
	if (transfer) {
		value = data;
		transfer = false;
	}
	return value;
}

byte V9990CmdEngine::logOp(byte src, byte dest)
{
	return logOp(src, dest, 255);
}

byte V9990CmdEngine::logOp(byte src, byte dest, byte mask)
{
	byte value = 0;
	
	if (!((LOG & 0x10) && (src == 0))) {
		for (int i = 0; i < 8; ++i) {
			value >>= 1;
			if (mask & 1) {
				int shift = ((src & 1) << 1) | (dest & 1);
				if (LOG & (1 << shift)) {
					value |= 0x80;
				}
			} else {
				if (dest & 1) {
					value |= 0x80;
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
	vdp->cmdReady();
}

} // namespace openmsx
