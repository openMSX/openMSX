// $Id$

#include "openmsx.hh"
#include "V9990CmdEngine.hh"

namespace openmsx {

// 2 bpp --------------------------------------------------------------
inline uint V9990CmdEngine::V9990Bpp2::addressOf(int x, int y, int imageWidth)
{
	return ((x/PIXELS_PER_BYTE) % imageWidth) + y * imageWidth;
}

inline byte V9990CmdEngine::V9990Bpp2::writeMask(int x) {
	return (byte) (~x & ADDRESS_MASK);
}

inline word V9990CmdEngine::V9990Bpp2::point(V9990VRAM *vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline void V9990CmdEngine::V9990Bpp2::pset(V9990VRAM *vram,
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

inline word V9990CmdEngine::V9990Bpp4::point(V9990VRAM *vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	int  shift = (~x & ADDRESS_MASK) * BITS_PER_PIXEL;
	return ((value >> shift) & MASK);
}

inline void V9990CmdEngine::V9990Bpp4::pset(V9990VRAM *vram,
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

inline word V9990CmdEngine::V9990Bpp8::point(V9990VRAM *vram,
                                             int x, int y, int imageWidth)
{
	word value = (word)(vram->readVRAM(addressOf(x,y, imageWidth)));
	return (value);
}

inline byte V9990CmdEngine::V9990Bpp8::writeMask(int x) {
	return 0x00;
}

inline void V9990CmdEngine::V9990Bpp8::pset(V9990VRAM *vram,
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

inline word V9990CmdEngine::V9990Bpp16::point(V9990VRAM *vram,
                                             int x, int y, int imageWidth)
{
	int addr = addressOf(x,y, imageWidth);
	word value = vram->readVRAM(addr)+(vram->readVRAM(addr+1) << 8);
	return value;
}

inline void V9990CmdEngine::V9990Bpp16::pset(V9990VRAM *vram,
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
	this->vdp = vdp_;
	//this->vram = vdp_->getVRAM();

	V9990CmdEngine::CmdSTOP* stopCmd =
		new V9990CmdEngine::CmdSTOP(this, vdp_->getVRAM());

	for(int mode = 0; mode < (BP2+1); mode++) {
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
	currentCommand = (V9990Cmd *)NULL;
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
	currentCommand = (V9990Cmd *) NULL;

	transfer = false;
}

void V9990CmdEngine::setCmdReg(byte reg, byte value, const EmuTime& time)
{
	PRT_DEBUG("[" << time << "] V9990CmdEngine::setCmdReg(" 
	          << dec << (int) reg << "," << (int) value << ")");
	//sync(time);
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
		break;
	case 13: // LOGOP
		LOG = value & 0x1F;
		break;
	case 14: // write mask low
	case 15: // write mask high
	case 16: // Font color - FG low
	case 17: // Font color - FG high
	case 18: // Font color - BG low
	case 19: // Font color - BG high
		break;
	case 20: // CMD
		{
			int cmd = (value >> 4) & 0x0F;
			if(currentCommand) {
				// Do Something to stop the running command
			}
			currentCommand = commands[cmd][vdp->getColorMode()];
			if(currentCommand) currentCommand->start(time);
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
	engine = engine_;
	vram   = vram_;
}

V9990CmdEngine::V9990Cmd::~V9990Cmd()
{
}

// ====================================================================
// STOP

V9990CmdEngine::CmdSTOP::CmdSTOP(V9990CmdEngine* engine,
                                 V9990VRAM*      vram)
	: V9990Cmd(engine, vram)
{
	
}

void V9990CmdEngine::CmdSTOP::start(const EmuTime& time)
{
	engine->transfer = false;
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
	engine->address = Mode::addressOf(engine->DX,
	                                  engine->DY,
									  engine->vdp->getImageWidth());
	engine->ANX = engine->NX;
	engine->ANY = engine->NY;
	if(Mode::PIXELS_PER_BYTE != 0) {
		/* <= 8bpp */
		
	}
}

void V9990CmdEngine::CmdLMMC<V9990CmdEngine::V9990Bpp16>::execute(const EmuTime& time)
{
	byte value = 0;
	int  dx    = (engine->ARG & 0x04)? -1: 1;
	int  dy    = (engine->ARG & 0x08)? -1: 1;

	if(engine->transfer) {
		engine->transfer = false;
		int width = engine->vdp->getImageWidth();

		value = vram->readVRAM(engine->address);
		value = engine->logOp(engine->data, value);
		vram->writeVRAM(engine->address, value);
		if(! (++(engine->address) & 0x01)) {
			engine->DX += dx;
			if(! (--(engine->ANX))) {
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if(! (--(engine->ANY))) {
					engine->cmdReady();
				} else {
					engine->ANX = engine->NX;
					engine->address += ((width - engine->ANX) << 1);
				}
			}
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdLMMC<Mode>::execute(const EmuTime& time)
{
	byte value = 0;
	int  dx    = (engine->ARG & 0x04)? -1: 1;
	int  dy    = (engine->ARG & 0x08)? -1: 1;

	if(engine->transfer) {
		int width = engine->vdp->getImageWidth()/Mode::PIXELS_PER_BYTE;
		byte data = engine->data;
		for(int i = 0; (engine->ANY > 0) && 
		               (i < Mode::PIXELS_PER_BYTE); i++) {
			value = (byte)(Mode::point(vram,
			                            engine->DX, engine->DY, width));
			value = engine->logOp((data >> (8 - Mode::BITS_PER_PIXEL)),
			                      value,
								  Mode::MASK);
			Mode::pset(vram, engine->DX, engine->DY, width, value);
			engine->DX += dx;
			if(!--(engine->ANX)) {
				engine->DX -= (engine->NX * dx);
				engine->DY += dy;
				if(!--(engine->ANY)) {
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
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::execute(const EmuTime& time)
{
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
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::execute(const EmuTime& time)
{
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
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::execute(const EmuTime& time)
{
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
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::execute(const EmuTime& time)
{
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
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::execute(const EmuTime& time)
{
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
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::execute(const EmuTime& time)
{
}

// ====================================================================
// CmdEngine methods

void V9990CmdEngine::setCmdData(byte value, const EmuTime& time)
{
	if(transfer && currentCommand) {
		currentCommand->execute(time);
	}
	data = value;
	transfer = true;
}

byte V9990CmdEngine::getCmdData(const EmuTime& time)
{
	byte value = 0xFF;

	//sync(time);
	if(transfer) {
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
	
	if(!((LOG & 0x10) && (dest == 0))) {
		for(int i = 0; i < 8; i++) {
			value >>= 1;
			value &= 0x7F;
			if(mask & 1) {
				int shift = ((src & 1) << 1) | (dest & 1);
				if(LOG & (1 << shift)) {
					value |= 0x80;
				}
			} else {
				if(src & 1) {
					value |= 0x80;
				}
			}
			src  >>= 1;
			dest >>= 1;
			mask >>= 1;
		}
	} else { 
		value = src;
	}

	return value;
}

void V9990CmdEngine::cmdReady(void)
{
	currentCommand = (V9990Cmd *) NULL;
}
}
