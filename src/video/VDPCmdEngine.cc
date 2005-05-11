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

/*
 About NX, NY
  - for block commands NX = 0 is equivalent to NX = 512 (TODO recheck this)
    and NY = 0 is equivalent to NY = 1024
  - when NX or NY is too large and the VDP command hits the border, the
    following happens:
     - when the left or right border is hit, the line terminates
     - when the top border is hit (line 0) the command terminates
     - when the bottom border (line 511 or 1023) the command continues
       (wraps to the top)
  - in 512 lines modes (e.g. screen 7) NY is NOT limited to 512, so when
    NY > 512, part of the screen is overdrawn twice
  - in 256 columns modes (e.g. screen 5) when "SX/DX >= 256", only 1 element
    (pixel or byte) is processed per horizontal line. The real x-ccordinate
    is "SX/DX & 255".
*/

#include <cstdio>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <memory>
#include "VDPCmdEngine.hh"
#include "EmuTime.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPSettings.hh"
#include "BooleanSetting.hh"

using std::min;
using std::max;
using std::auto_ptr;

namespace openmsx {

// Constants:
const byte DIY = 0x08;
const byte DIX = 0x04;
const byte EQ  = 0x02;
const byte MAJ = 0x01;

// Timing tables:
//               Sprites:    On   On   Off  Off
//               Screen:     Off  On   Off  On
const int SRCH_TIMING[5] = {  92, 125,  92,  92, 0 };
const int LINE_TIMING[5] = { 120, 147, 120, 132, 0 };
const int HMMV_TIMING[5] = {  49,  65,  49,  62, 0 };
const int LMMV_TIMING[5] = {  98, 137,  98, 124, 0 };
const int YMMM_TIMING[5] = {  65, 125,  65,  68, 0 };
const int HMMM_TIMING[5] = {  92, 136,  92,  97, 0 };
const int LMMM_TIMING[5] = { 129, 197, 129, 132, 0 };

// Inline methods first, to make sure they are actually inlined:

template <class Mode>
static inline word clipNX_1_pixel(word DX, word NX, byte ARG)
{
	if (DX >= Mode::PIXELS_PER_LINE) {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & DIX)
		? min(NX, (word)(DX + 1))
		: min(NX, (word)(Mode::PIXELS_PER_LINE - DX));
}

template <class Mode>
static inline word clipNX_1_byte(word DX, word NX, byte ARG)
{
	static const word BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if (BYTES_PER_LINE <= DX) {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & DIX)
		? min(NX, (word)(DX + 1))
		: min(NX, (word)(BYTES_PER_LINE - DX));
}

template <class Mode>
static inline word clipNX_2_pixel(word SX, word DX, word NX, byte ARG)
{
	if (SX >= Mode::PIXELS_PER_LINE || DX >= Mode::PIXELS_PER_LINE) {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & DIX)
		? min(NX, (word)(min(SX, DX) + 1))
		: min(NX, (word)(Mode::PIXELS_PER_LINE - max(SX, DX)));
}

template <class Mode>
static inline word clipNX_2_byte(word SX, word DX, word NX, byte ARG)
{
	static const word BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	SX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if ((BYTES_PER_LINE <= SX) || (BYTES_PER_LINE <= DX)) {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & DIX)
		? min(NX, (word)(min(SX, DX) + 1))
		: min(NX, (word)(BYTES_PER_LINE - max(SX, DX)));
}

static inline word clipNY_1(word DY, word NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & DIY) ? min(NY, (word)(DY + 1)) : NY;
}

static inline word clipNY_2(word SY, word DY, word NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & DIY) ? min(NY, (word)(min(SY, DY) + 1)) : NY;
}

// Graphic4Mode:

inline int VDPCmdEngine::Graphic4Mode::addressOf(int x, int y)
{
	return (((y & 1023) << 7) + ((x & 255) >> 1));
}

inline byte VDPCmdEngine::Graphic4Mode::point(VDPVRAM* vram, int x, int y)
{
	return ( vram->cmdReadWindow.readNP(addressOf(x, y))
		>> (((~x) & 1) << 2) ) & 15;
}

inline void VDPCmdEngine::Graphic4Mode::pset(
	const EmuTime& time, VDPVRAM* vram, int x, int y, byte colour, LogOp* op )
{
	byte sh = ((~x) & 1) << 2;
	op->pset(time, vram, addressOf(x, y), colour << sh, ~(15 << sh));
}

// Graphic5Mode:

inline int VDPCmdEngine::Graphic5Mode::addressOf(int x, int y)
{
	return (((y & 1023) << 7) + ((x & 511) >> 2));
}

inline byte VDPCmdEngine::Graphic5Mode::point(VDPVRAM* vram, int x, int y)
{
	return ( vram->cmdReadWindow.readNP(addressOf(x, y))
		>> (((~x) & 3) << 1) ) & 3;
}

inline void VDPCmdEngine::Graphic5Mode::pset(
	const EmuTime& time, VDPVRAM* vram, int x, int y, byte colour, LogOp* op )
{
	byte sh = ((~x) & 3) << 1;
	op->pset(time, vram, addressOf(x, y), colour << sh, ~(3 << sh));
}

// Graphic6Mode:

inline int VDPCmdEngine::Graphic6Mode::addressOf(int x, int y)
{
	return (((x & 2) << 15) + ((y & 511) << 7) + ((x & 511) >> 2));
}

inline byte VDPCmdEngine::Graphic6Mode::point(VDPVRAM* vram, int x, int y)
{
	return ( vram->cmdReadWindow.readNP(addressOf(x, y))
		>> (((~x) & 1) << 2) ) & 15;
}

inline void VDPCmdEngine::Graphic6Mode::pset(
	const EmuTime& time, VDPVRAM* vram, int x, int y, byte colour, LogOp* op )
{
	byte sh = ((~x) & 1) << 2;
	op->pset(time, vram, addressOf(x, y), colour << sh, ~(15 << sh));
}

// Graphic7Mode:

inline int VDPCmdEngine::Graphic7Mode::addressOf(int x, int y)
{
	return (((x & 1) << 16) + ((y & 511) << 7) + ((x & 255) >> 1));
}

inline byte VDPCmdEngine::Graphic7Mode::point(VDPVRAM* vram, int x, int y)
{
	return vram->cmdReadWindow.readNP(addressOf(x, y));
}

inline void VDPCmdEngine::Graphic7Mode::pset(
	const EmuTime& time, VDPVRAM* vram, int x, int y, byte colour, LogOp* op )
{
	op->pset(time, vram, addressOf(x, y), colour, 0);
}

// Logical operations:

typedef VDPCmdEngine::LogOp LogOp;

class DummyOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& /*time*/, VDPVRAM* /*vram*/, int /*addr*/,
		byte /*colour*/, byte /*mask*/)
	{
		// Undefined logical operations do nothing.
	}
};

class ImpOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram, int addr, byte colour, byte mask)
	{
		vram->cmdWrite(addr,
			(vram->cmdWriteWindow.readNP(addr) & mask) | colour,
			time);
	}
};

class AndOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram, int addr, byte colour, byte mask)
	{
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) & (colour | mask),
			time);
	}
};

class OrOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram,
		int addr, byte colour, byte /*mask*/)
	{
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) | colour,
			time);
	}
};

class XorOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram,
		int addr, byte colour, byte /*mask*/)
	{
		vram->cmdWrite(addr,
			vram->cmdWriteWindow.readNP(addr) ^ colour,
			time);
	}
};

class NotOp: public LogOp {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram, int addr, byte colour, byte mask)
	{
		vram->cmdWrite(addr,
			(vram->cmdWriteWindow.readNP(addr) & mask) | ~(colour | mask),
			time);
	}
};

template <class Op>
class TransparentOp: public Op {
public:
	virtual void pset(
		const EmuTime& time, VDPVRAM* vram, int addr, byte colour, byte mask)
	{
		if (colour) Op::pset(time, vram, addr, colour, mask);
	}
};
typedef TransparentOp<ImpOp> TImpOp;
typedef TransparentOp<AndOp> TAndOp;
typedef TransparentOp<OrOp> TOrOp;
typedef TransparentOp<XorOp> TXorOp;
typedef TransparentOp<NotOp> TNotOp;

static auto_ptr<LogOp> operations[16] = {
	auto_ptr<LogOp>(new ImpOp()),   auto_ptr<LogOp>(new AndOp()),
	auto_ptr<LogOp>(new OrOp()),    auto_ptr<LogOp>(new XorOp()),
	auto_ptr<LogOp>(new NotOp()),   auto_ptr<LogOp>(new DummyOp()),
	auto_ptr<LogOp>(new DummyOp()), auto_ptr<LogOp>(new DummyOp()),
	auto_ptr<LogOp>(new TImpOp()),  auto_ptr<LogOp>(new TAndOp()),
	auto_ptr<LogOp>(new TOrOp()),   auto_ptr<LogOp>(new TXorOp()),
	auto_ptr<LogOp>(new TNotOp()),  auto_ptr<LogOp>(new DummyOp()),
	auto_ptr<LogOp>(new DummyOp()), auto_ptr<LogOp>(new DummyOp()),
};

// Construction and destruction:

template <template <class Mode> class Command>
void VDPCmdEngine::createEngines(int cmd) {
	commands[cmd][0] = new Command<Graphic4Mode>(this, vram);
	commands[cmd][1] = new Command<Graphic5Mode>(this, vram);
	commands[cmd][2] = new Command<Graphic6Mode>(this, vram);
	commands[cmd][3] = new Command<Graphic7Mode>(this, vram);
}

VDPCmdEngine::VDPCmdEngine(VDP* vdp_)
	: vdp(vdp_)
	, cmdTraceSetting(new BooleanSetting("vdpcmdtrace",
	                         "VDP command tracing on/off", false))
{
	vram = vdp->getVRAM();

	status = 0;
	transfer = false;
	SX = SY = DX = DY = NX = NY = 0;
	COL = ARG = CMD = LOG = 0;

	AbortCmd* abort = new AbortCmd(this, vram);
	for (int cmd = 0x0; cmd < 0x4; cmd++) {
		for (int mode = 0; mode < 4; mode++) {
			commands[cmd][mode] = abort;
		}
	}
	createEngines<PointCmd>(0x4);
	createEngines<PsetCmd>(0x5);
	createEngines<SrchCmd>(0x6);
	createEngines<LineCmd>(0x7);
	createEngines<LmmvCmd>(0x8);
	createEngines<LmmmCmd>(0x9);
	createEngines<LmcmCmd>(0xA);
	createEngines<LmmcCmd>(0xB);
	createEngines<HmmvCmd>(0xC);
	createEngines<HmmmCmd>(0xD);
	createEngines<YmmmCmd>(0xE);
	createEngines<HmmcCmd>(0xF);
	currentCommand = NULL;

	currentOperation = operations[LOG].get();

	brokenTiming = false;
	statusChangeTime = EmuTime::infinity;

	VDPSettings::instance().getCmdTiming()->addListener(this);
}

VDPCmdEngine::~VDPCmdEngine()
{
	VDPSettings::instance().getCmdTiming()->removeListener(this);

	// Abort command:
	delete commands[0][0];
	// Other commands:
	for (int cmd = 4; cmd < 16; cmd++) {
		for (int mode = 0; mode < 4; mode++) {
			delete commands[cmd][mode];
		}
	}
}

void VDPCmdEngine::reset(const EmuTime& time)
{
	status = 0;
	borderX = 0;
	scrMode = -1;
	for (int i = 0; i < 15; i++) {
		setCmdReg(i, 0, time);
	}

	updateDisplayMode(vdp->getDisplayMode(), time);
}

void VDPCmdEngine::update(const Setting* setting)
{
	brokenTiming = static_cast<const EnumSetting<bool>*>(setting)->getValue();
}

void VDPCmdEngine::setCmdReg(byte index, byte value, const EmuTime& time)
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
		// Note: Real VDP always resets TR, but for such a short time
		//       that the MSX won't notice it.
		// TODO: What happens on non-transfer commands?
		if (!currentCommand) status &= 0x7F;
		transfer = true;
		break;
	case 0x0D: // argument
		ARG = value;
		break;
	case 0x0E: // command
		LOG = value & 0x0F;
		CMD = value >> 4;
		executeCommand(time);
		break;
	default:
		assert(false);
	}
}

byte VDPCmdEngine::peekCmdReg(byte index)
{
	switch (index) {
	case 0x00: return SX & 0xFF;
	case 0x01: return SX >> 8;
	case 0x02: return SY & 0xFF;
	case 0x03: return SY >> 8;

	case 0x04: return DX & 0xFF;
	case 0x05: return DX >> 8;
	case 0x06: return DY & 0xFF;
	case 0x07: return DY >> 8;

	case 0x08: return NX & 0xFF;
	case 0x09: return NX >> 8;
	case 0x0A: return NY & 0xFF;
	case 0x0B: return NY >> 8;

	case 0x0C: return COL;
	case 0x0D: return ARG;
	case 0x0E: return (CMD << 4) | LOG;
	default: assert(false); return 0;
	}
}

void VDPCmdEngine::updateDisplayMode(DisplayMode mode, const EmuTime& time)
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
		if (currentCommand) {
			PRT_DEBUG("VDP mode switch while command in progress");
			if (newScrMode == -1) {
				// TODO: For now abort cmd in progress,
				//       later find out what really happens.
				//       At least CE remains high for a while,
				//       but it is not yet clear what happens in VRAM.
				commandDone(time);
			} else {
				VDPCmd* newCommand = commands[CMD][newScrMode];
				newCommand->copyProgressFrom(currentCommand);
				currentCommand = newCommand;
			}
		}
		sync(time);
		scrMode = newScrMode;
	}
}

void VDPCmdEngine::executeCommand(const EmuTime& time)
{
	// V9938 ops only work in SCREEN 5-8.
	// V9958 ops work in non SCREEN 5-8 when CMD bit is set
	if (scrMode < 0) {
		commandDone(time);
		return;
	}

	if (cmdTraceSetting->getValue()) {
		reportVdpCommand();
	}

	// Start command.
	status |= 0x01;
	currentCommand = commands[CMD][scrMode];
	currentOperation = operations[LOG].get();
	currentCommand->start(time);

	// Finish command now if instantaneous command timing is active.
	// Abort finishes on start, so currentCommand can be NULL.
	if (brokenTiming && currentCommand) {
		currentCommand->execute(time);
	}
}

void VDPCmdEngine::reportVdpCommand()
{
	const char* const COMMANDS[16] = {
		" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
		" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
	};
	const char* const OPS[16] = {
		"IMP ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
		"TIMP","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
	};

	std::cerr << "VDPCmd " << COMMANDS[CMD] << '-' << OPS[LOG]
		<<  '(' << (int)SX << ',' << (int)SY << ")->("
		        << (int)DX << ',' << (int)DY << ")," << (int)COL
		<< " [" << (int)((ARG & DIX) ? -NX : NX)
		<<  ',' << (int)((ARG & DIY) ? -NY : NY) << ']' << std::endl;
}

void VDPCmdEngine::commandDone(const EmuTime& time)
{
	// Note: TR is not reset yet; it is reset when S#2 is read next.
	status &= 0xFE;	// reset CE
	CMD = 0;
	currentCommand = NULL;
	statusChangeTime = EmuTime::infinity;
	vram->cmdReadWindow.disable(time);
	vram->cmdWriteWindow.disable(time);
}

// VDPCmd:

VDPCmdEngine::VDPCmd::VDPCmd(VDPCmdEngine* engine_, VDPVRAM* vram_)
	: engine(engine_), vram(vram_)
{
}

VDPCmdEngine::VDPCmd::~VDPCmd()
{
}

void VDPCmdEngine::VDPCmd::copyProgressFrom(VDPCmd* other)
{
	this->ASX = other->ASX;
	this->ADX = other->ADX;
	this->ANX = other->ANX;
	this->clock.reset(other->clock.getTime());
}

// Many VDP commands are executed in some kind of loop but
// essentially, there are only a few basic loop structures
// that are re-used. We define the loop structures that are
// re-used here so that they have to be entered only once.

// Loop over DX, DY.
#define post_x_y() \
		clock += delta; \
		ADX += TX; \
		if (--ANX == 0) { \
			engine->DY += TY; --(engine->NY); \
			ADX = engine->DX; ANX = NX; \
			if (--NY == 0) { \
				engine->commandDone(clock.getTime()); \
				break; \
			} \
		} \
	}

// Loop over SX, DX, SY, DY.
#define post_xxyy() \
		clock += delta; \
		ASX += TX; ADX += TX; \
		if (--ANX == 0) { \
			engine->SY += TY; engine->DY += TY; --(engine->NY); \
			ASX = engine->SX; ADX = engine->DX; ANX = NX; \
			if (--NY == 0) { \
				engine->commandDone(clock.getTime()); \
				break; \
			} \
		} \
	}

// ABORT

VDPCmdEngine::AbortCmd::AbortCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: VDPCmd(engine, vram)
{
}

void VDPCmdEngine::AbortCmd::start(const EmuTime& time)
{
	engine->commandDone(time);
}

void VDPCmdEngine::AbortCmd::execute(const EmuTime& /*time*/)
{
}


// POINT

template <class Mode>
VDPCmdEngine::PointCmd<Mode>::PointCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: VDPCmd(engine, vram)
{
}

template <class Mode>
void VDPCmdEngine::PointCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.disable(clock.getTime());

	engine->COL = Mode::point(vram, engine->SX, engine->SY);
	engine->commandDone(clock.getTime());
}

template <class Mode>
void VDPCmdEngine::PointCmd<Mode>::execute(const EmuTime& /*time*/)
{
}


// PSET

template <class Mode>
VDPCmdEngine::PsetCmd<Mode>::PsetCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: VDPCmd(engine, vram)
{
}

template <class Mode>
void VDPCmdEngine::PsetCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());

	byte col = engine->COL & Mode::COLOUR_MASK;
	Mode::pset(clock.getTime(), vram,
		engine->DX, engine->DY, col, engine->currentOperation);
	engine->commandDone(clock.getTime());
}

template <class Mode>
void VDPCmdEngine::PsetCmd<Mode>::execute(const EmuTime& /*time*/)
{
}


// SRCH

template <class Mode>
VDPCmdEngine::SrchCmd<Mode>::SrchCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: VDPCmd(engine, vram)
{
}

template <class Mode>
void VDPCmdEngine::SrchCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.disable(clock.getTime());
	ASX = engine->SX;
	engine->statusChangeTime = EmuTime::zero; // we can find it any moment
}

template <class Mode>
void VDPCmdEngine::SrchCmd<Mode>::execute(const EmuTime& time)
{
	byte CL = engine->COL & Mode::COLOUR_MASK;
	char TX = (engine->ARG & DIX) ? -1 : 1;
	bool AEQ = (engine->ARG & EQ) != 0; // TODO: Do we look for "==" or "!="?
	int delta = SRCH_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		if ((Mode::point(vram, ASX, engine->SY) == CL) ^ AEQ) {
			clock += delta;
			engine->status |= 0x10; // border detected
			engine->commandDone(clock.getTime());
			engine->borderX = 0xFE00 | ASX;
			break;
		}
		clock += delta;
		if ((ASX += TX) & Mode::PIXELS_PER_LINE) {
			engine->status &= 0xEF; // border not detected
			engine->commandDone(clock.getTime());
			engine->borderX = 0xFE00 | ASX;
			break;
		}
	}
}


// LINE

template <class Mode>
VDPCmdEngine::LineCmd<Mode>::LineCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: VDPCmd(engine, vram)
{
}

template <class Mode>
void VDPCmdEngine::LineCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	ASX = ((engine->NX - 1) >> 1);
	ADX = engine->DX;
	ANX = 0;
	engine->statusChangeTime = EmuTime::zero; // TODO can still be optimized
}

template <class Mode>
void VDPCmdEngine::LineCmd<Mode>::execute(const EmuTime& time)
{
	byte CL = engine->COL & Mode::COLOUR_MASK;
	char TX = (engine->ARG & DIX) ? -1 : 1;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	int delta = LINE_TIMING[engine->getTiming()];

	if ((engine->ARG & MAJ) == 0) {
		// X-Axis is major direction.
		while (clock.before(time)) {
			Mode::pset(clock.getTime(), vram,
				ADX, engine->DY, CL, engine->currentOperation);
			clock += delta;
			ADX += TX;
			if (ASX < engine->NY) {
				ASX += engine->NX;
				engine->DY += TY;
			}
			ASX -= engine->NY;
			ASX &= 1023; // mask to 10 bits range
			if (ANX++ == engine->NX || (ADX & Mode::PIXELS_PER_LINE)) {
				engine->commandDone(clock.getTime());
				break;
			}
		}
	} else {
		// Y-Axis is major direction.
		while (clock.before(time)) {
			Mode::pset(clock.getTime(), vram,
				ADX, engine->DY, CL, engine->currentOperation);
			clock += delta;
			engine->DY += TY;
			if (ASX < engine->NY) {
				ASX += engine->NX;
				ADX += TX;
			}
			ASX -= engine->NY;
			ASX &= 1023; // mask to 10 bits range
			if (ANX++ == engine->NX || (ADX & Mode::PIXELS_PER_LINE)) {
				engine->commandDone(clock.getTime());
				break;
			}
		}
	}
}


// BlockCmd

VDPCmdEngine::BlockCmd::BlockCmd(VDPCmdEngine* engine, VDPVRAM* vram,
		                 const int* timing_)
	: VDPCmd(engine, vram), timing(timing_)
{
}

void VDPCmdEngine::BlockCmd::calcFinishTime(word NX, word NY)
{
	if (engine->currentCommand) {
		int ticks = ((NX * (NY - 1)) + ANX) * timing[engine->getTiming()];
		engine->statusChangeTime = clock + ticks;
	}
}


// LMMV

template <class Mode>
VDPCmdEngine::LmmvCmd<Mode>::LmmvCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, LMMV_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::LmmvCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	ADX = engine->DX;
	ANX = NX;
	calcFinishTime(NX, NY);
}

template <class Mode>
void VDPCmdEngine::LmmvCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX) ? -1 : 1;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ADX, ANX, engine->ARG);
	byte CL = engine->COL & Mode::COLOUR_MASK;
	int delta = LMMV_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		Mode::pset(clock.getTime(), vram,
			ADX, engine->DY, CL, engine->currentOperation);
	post_x_y()

	calcFinishTime(NX, NY);
}


// LMMM

template <class Mode>
VDPCmdEngine::LmmmCmd<Mode>::LmmmCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, LMMM_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::LmmmCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_2_pixel<Mode>(
		engine->SX, engine->DX, engine->NX, engine->ARG );
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	ASX = engine->SX;
	ADX = engine->DX;
	ANX = NX;
	calcFinishTime(NX, NY);
}

template <class Mode>
void VDPCmdEngine::LmmmCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_2_pixel<Mode>(
		engine->SX, engine->DX, engine->NX, engine->ARG );
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX) ? -1 : 1;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_2_pixel<Mode>(ASX, ADX, ANX, engine->ARG);
	int delta = LMMM_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		Mode::pset(clock.getTime(), vram,
			ADX, engine->DY, Mode::point(vram, ASX, engine->SY),
			engine->currentOperation);
	post_xxyy()

	calcFinishTime(NX, NY);
}


// LMCM

template <class Mode>
VDPCmdEngine::LmcmCmd<Mode>::LmcmCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, LMMV_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::LmcmCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.disable(clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->SX, engine->NX, engine->ARG);
	ASX = engine->SX;
	ANX = NX;
	engine->statusChangeTime = EmuTime::zero;
	engine->transfer = true;
	engine->status |= 0x80;
}

template <class Mode>
void VDPCmdEngine::LmcmCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->SX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->SY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX) ? -1 : 1;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ASX, ANX, engine->ARG);

	if (engine->transfer) {
		engine->COL = Mode::point(vram, ASX, engine->SY);
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += LMMV_TIMING[engine->getTiming()];
		engine->transfer = false;
		ASX += TX; --ANX;
		if (ANX == 0) {
			engine->SY += TY; --(engine->NY);
			ASX = engine->SX; ANX = NX;
			if (--NY == 0) {
				engine->commandDone(time);
			}
		}
	}
}


// LMMC

template <class Mode>
VDPCmdEngine::LmmcCmd<Mode>::LmmcCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, LMMV_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::LmmcCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->DX, engine->NX, engine->ARG);
	ADX = engine->DX;
	ANX = NX;
	engine->statusChangeTime = EmuTime::zero;
	engine->transfer = true;
	engine->status |= 0x80;
}

template <class Mode>
void VDPCmdEngine::LmmcCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_pixel<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX) ? -1 : 1;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ADX, ANX, engine->ARG);

	if (engine->transfer) {
		byte col = engine->COL & Mode::COLOUR_MASK;
		// TODO: Write time is inaccurate.
		clock.reset(time);
		Mode::pset(clock.getTime(), vram,
			ADX, engine->DY, col, engine->currentOperation);
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += LMMV_TIMING[engine->getTiming()];
		engine->transfer = false;

		ADX += TX; --ANX;
		if (ANX == 0) {
			engine->DY += TY; --(engine->NY);
			ADX = engine->DX; ANX = NX;
			if (--NY == 0) {
				engine->commandDone(time);
			}
		}
	}
}


// HMMV

template <class Mode>
VDPCmdEngine::HmmvCmd<Mode>::HmmvCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, HMMV_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::HmmvCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	ADX = engine->DX;
	ANX = NX;
	calcFinishTime(NX, NY);
}

template <class Mode>
void VDPCmdEngine::HmmvCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(
		ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine->ARG );
	int delta = HMMV_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		vram->cmdWrite(
			Mode::addressOf(ADX, engine->DY), engine->COL, clock.getTime());
	post_x_y()

	calcFinishTime(NX, NY);
}


// HMMM

template <class Mode>
VDPCmdEngine::HmmmCmd<Mode>::HmmmCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, HMMM_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::HmmmCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_2_byte<Mode>(
		engine->SX, engine->DX, engine->NX, engine->ARG );
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	ASX = engine->SX;
	ADX = engine->DX;
	ANX = NX;
	calcFinishTime(NX, NY);
}

template <class Mode>
void VDPCmdEngine::HmmmCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_2_byte<Mode>(
		engine->SX, engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_2_byte<Mode>(
		ASX, ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine->ARG );
	int delta = HMMM_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		vram->cmdWrite(
			Mode::addressOf(ADX, engine->DY),
			vram->cmdReadWindow.readNP(Mode::addressOf(ASX, engine->SY)),
			clock.getTime()
			);
	post_xxyy()

	calcFinishTime(NX, NY);
}


// YMMM

template <class Mode>
VDPCmdEngine::YmmmCmd<Mode>::YmmmCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, YMMM_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::YmmmCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, 512, engine->ARG);
		// large enough so that it gets clipped
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	ADX = engine->DX;
	ANX = NX;
	calcFinishTime(NX, NY);
}

template <class Mode>
void VDPCmdEngine::YmmmCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, 512, engine->ARG);
		// large enough so that it gets clipped
	word NY = clipNY_2(engine->SY, engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(ADX, 512, engine->ARG);
	int delta = YMMM_TIMING[engine->getTiming()];

	while (clock.before(time)) {
		vram->cmdWrite(
			Mode::addressOf(ADX, engine->DY),
			vram->cmdReadWindow.readNP(Mode::addressOf(ADX, engine->SY)),
			clock.getTime()
			);
	post_xxyy()

	calcFinishTime(NX, NY);
}


// HMMC

template <class Mode>
VDPCmdEngine::HmmcCmd<Mode>::HmmcCmd(VDPCmdEngine* engine, VDPVRAM* vram)
	: BlockCmd(engine, vram, HMMV_TIMING)
{
}

template <class Mode>
void VDPCmdEngine::HmmcCmd<Mode>::start(const EmuTime& time)
{
	clock.reset(time);
	vram->cmdReadWindow.disable(clock.getTime());
	vram->cmdWriteWindow.setMask(0x1FFFF, -1 << 17, clock.getTime());
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, engine->NX, engine->ARG);
	ADX = engine->DX;
	ANX = NX;
	engine->statusChangeTime = EmuTime::zero;
	engine->transfer = true;
	engine->status |= 0x80;
}

template <class Mode>
void VDPCmdEngine::HmmcCmd<Mode>::execute(const EmuTime& time)
{
	engine->NY &= 1023;
	word NX = clipNX_1_byte<Mode>(engine->DX, engine->NX, engine->ARG);
	word NY = clipNY_1(engine->DY, engine->NY, engine->ARG);
	char TX = (engine->ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	char TY = (engine->ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(
		ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine->ARG );

	if (engine->transfer) {
		// TODO: Write time is inaccurate.
		vram->cmdWrite(Mode::addressOf(ADX, engine->DY), engine->COL, time);
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += HMMV_TIMING[engine->getTiming()];
		engine->transfer = false;

		ADX += TX; --ANX;
		if (ANX == 0) {
			engine->DY += TY; --(engine->NY);
			ADX = engine->DX; ANX = NX;
			if (--NY == 0) {
				engine->commandDone(clock.getTime());
			}
		}
	}
}

} // namespace openmsx
