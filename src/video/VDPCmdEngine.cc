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

#include "VDPCmdEngine.hh"
#include "EmuTime.hh"
#include "VDPVRAM.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "RenderSettings.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <iostream>
#include <cassert>
#include <algorithm>

using std::min;
using std::max;

namespace openmsx {

// Constants:
const byte MXD = 0x20;
const byte MXS = 0x10;
const byte DIY = 0x08;
const byte DIX = 0x04;
const byte EQ  = 0x02;
const byte MAJ = 0x01;

// Timing tables:
//                    Sprites:    On   On   Off  Off
//                    Screen:     Off  On   Off  On
const unsigned SRCH_TIMING[5] = {  92, 125,  92,  92, 0 };
const unsigned LINE_TIMING[5] = { 120, 147, 120, 132, 0 };
const unsigned HMMV_TIMING[5] = {  49,  65,  49,  62, 0 };
const unsigned LMMV_TIMING[5] = {  98, 137,  98, 124, 0 };
const unsigned YMMM_TIMING[5] = {  65, 125,  65,  68, 0 };
const unsigned HMMM_TIMING[5] = {  92, 136,  92,  97, 0 };
const unsigned LMMM_TIMING[5] = { 129, 197, 129, 132, 0 };

// Inline methods first, to make sure they are actually inlined:

template <typename Mode>
static inline unsigned clipNX_1_pixel(unsigned DX, unsigned NX, byte ARG)
{
	if (unlikely(DX >= Mode::PIXELS_PER_LINE)) {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & DIX)
		? min(NX, DX + 1)
		: min(NX, Mode::PIXELS_PER_LINE - DX);
}

template <typename Mode>
static inline unsigned clipNX_1_byte(unsigned DX, unsigned NX, byte ARG)
{
	static const unsigned BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if (unlikely(BYTES_PER_LINE <= DX)) {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & DIX)
		? min(NX, DX + 1)
		: min(NX, BYTES_PER_LINE - DX);
}

template <typename Mode>
static inline unsigned clipNX_2_pixel(unsigned SX, unsigned DX, unsigned NX, byte ARG)
{
	if (unlikely(SX >= Mode::PIXELS_PER_LINE) ||
	    unlikely(DX >= Mode::PIXELS_PER_LINE)) {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & DIX)
		? min(NX, min(SX, DX) + 1)
		: min(NX, Mode::PIXELS_PER_LINE - max(SX, DX));
}

template <typename Mode>
static inline unsigned clipNX_2_byte(unsigned SX, unsigned DX, unsigned NX, byte ARG)
{
	static const unsigned BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	SX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if (unlikely(BYTES_PER_LINE <= SX) ||
	    unlikely(BYTES_PER_LINE <= DX)) {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & DIX)
		? min(NX, min(SX, DX) + 1)
		: min(NX, BYTES_PER_LINE - max(SX, DX));
}

static inline unsigned clipNY_1(unsigned DY, unsigned NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & DIY) ? min(NY, DY + 1) : NY;
}

static inline unsigned clipNY_2(unsigned SY, unsigned DY, unsigned NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & DIY) ? min(NY, min(SY, DY) + 1) : NY;
}


struct IncrByteAddr4;
struct IncrByteAddr5;
struct IncrByteAddr6;
struct IncrByteAddr7;
struct IncrPixelAddr4;
struct IncrPixelAddr5;
struct IncrPixelAddr6;
struct IncrMask4;
struct IncrMask5;
struct IncrMask7;
struct IncrShift4;
struct IncrShift5;
struct IncrShift7;
typedef IncrByteAddr7 IncrPixelAddr7;
typedef IncrMask4 IncrMask6;
typedef IncrShift4 IncrShift6;


template <typename LogOp> static void psetFast(
	EmuTime::param time, VDPVRAM& vram, unsigned addr,
	byte color, byte mask, LogOp op)
{
	op(time, vram, addr, color, mask);
}

/** Represents V9938 Graphic 4 mode (SCREEN5).
  */
struct Graphic4Mode
{
	typedef IncrByteAddr4 IncrByteAddr;
	typedef IncrPixelAddr4 IncrPixelAddr;
	typedef IncrMask4 IncrMask;
	typedef IncrShift4 IncrShift;
	static const byte COLOR_MASK = 0x0F;
	static const byte PIXELS_PER_BYTE = 2;
	static const byte PIXELS_PER_BYTE_SHIFT = 1;
	static const unsigned PIXELS_PER_LINE = 256;
	static inline unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static inline byte point(VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template <typename LogOp>
	static inline void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned y, bool extVRAM, byte color, LogOp op);
	static inline byte duplicate(byte color);
};

inline unsigned Graphic4Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	return likely(!extVRAM)
		? (((y & 1023) << 7) | ((x & 255) >> 1))
		: (((y &  511) << 7) | ((x & 255) >> 1) | 0x20000);
}

inline byte Graphic4Mode::point(
	VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return ( vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 1) << 2) ) & 15;
}

template <typename LogOp>
inline void Graphic4Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned y,
	bool extVRAM, byte color, LogOp op)
{
	byte sh = ((~x) & 1) << 2;
	op(time, vram, addressOf(x, y, extVRAM), color << sh, ~(15 << sh));
}

inline byte Graphic4Mode::duplicate(byte color)
{
	assert((color & 0xF0) == 0);
	return color | (color << 4);
}

/** Represents V9938 Graphic 5 mode (SCREEN6).
  */
struct Graphic5Mode
{
	typedef IncrByteAddr5 IncrByteAddr;
	typedef IncrPixelAddr5 IncrPixelAddr;
	typedef IncrMask5 IncrMask;
	typedef IncrShift5 IncrShift;
	static const byte COLOR_MASK = 0x03;
	static const byte PIXELS_PER_BYTE = 4;
	static const byte PIXELS_PER_BYTE_SHIFT = 2;
	static const unsigned PIXELS_PER_LINE = 512;
	static inline unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static inline byte point(VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template <typename LogOp>
	static inline void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned y, bool extVRAM, byte color, LogOp op);
	static inline byte duplicate(byte color);
};

inline unsigned Graphic5Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	return likely(!extVRAM)
		? (((y & 1023) << 7) | ((x & 511) >> 2))
		: (((y &  511) << 7) | ((x & 511) >> 2) | 0x20000);
}

inline byte Graphic5Mode::point(
	VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return ( vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 3) << 1) ) & 3;
}

template <typename LogOp>
inline void Graphic5Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned y,
	bool extVRAM, byte color, LogOp op)
{
	byte sh = ((~x) & 3) << 1;
	op(time, vram, addressOf(x, y, extVRAM), color << sh, ~(3 << sh));
}

inline byte Graphic5Mode::duplicate(byte color)
{
	assert((color & 0xFC) == 0);
	color |= color << 2;
	color |= color << 4;
	return color;
}

/** Represents V9938 Graphic 6 mode (SCREEN7).
  */
struct Graphic6Mode
{
	typedef IncrByteAddr6 IncrByteAddr;
	typedef IncrPixelAddr6 IncrPixelAddr;
	typedef IncrMask6 IncrMask;
	typedef IncrShift6 IncrShift;
	static const byte COLOR_MASK = 0x0F;
	static const byte PIXELS_PER_BYTE = 2;
	static const byte PIXELS_PER_BYTE_SHIFT = 1;
	static const unsigned PIXELS_PER_LINE = 512;
	static inline unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static inline byte point(VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template <typename LogOp>
	static inline void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned y, bool extVRAM, byte color, LogOp op);
	static inline byte duplicate(byte color);
};

inline unsigned Graphic6Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	return likely(!extVRAM)
		? (((x & 2) << 15) | ((y & 511) << 7) | ((x & 511) >> 2))
		: (0x20000         | ((y & 511) << 7) | ((x & 511) >> 2));
}

inline byte Graphic6Mode::point(
	VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return ( vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 1) << 2) ) & 15;
}

template <typename LogOp>
inline void Graphic6Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned y,
	bool extVRAM, byte color, LogOp op)
{
	byte sh = ((~x) & 1) << 2;
	op(time, vram, addressOf(x, y, extVRAM), color << sh, ~(15 << sh));
}

inline byte Graphic6Mode::duplicate(byte color)
{
	assert((color & 0xF0) == 0);
	return color | (color << 4);
}

/** Represents V9938 Graphic 7 mode (SCREEN8).
  */
struct Graphic7Mode
{
	typedef IncrByteAddr7 IncrByteAddr;
	typedef IncrPixelAddr7 IncrPixelAddr;
	typedef IncrMask7 IncrMask;
	typedef IncrShift7 IncrShift;
	static const byte COLOR_MASK = 0xFF;
	static const byte PIXELS_PER_BYTE = 1;
	static const byte PIXELS_PER_BYTE_SHIFT = 0;
	static const unsigned PIXELS_PER_LINE = 256;
	static inline unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static inline byte point(VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template <typename LogOp>
	static inline void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned y, bool extVRAM, byte color, LogOp op);
	static inline byte duplicate(byte color);
};

inline unsigned Graphic7Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	return likely(!extVRAM)
		? (((x & 1) << 16) | ((y & 511) << 7) | ((x & 255) >> 1))
		: (0x20000         | ((y & 511) << 7) | ((x & 255) >> 1));
}

inline byte Graphic7Mode::point(
	VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM));
}

template <typename LogOp>
inline void Graphic7Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned y,
	bool extVRAM, byte color, LogOp op)
{
	op(time, vram, addressOf(x, y, extVRAM), color, 0);
}

inline byte Graphic7Mode::duplicate(byte color)
{
	return color;
}

/** Incremental address calculation (byte based, no extended VRAM)
 */
struct IncrByteAddr4
{
	IncrByteAddr4(unsigned x, unsigned y, int /*tx*/)
	{
		addr = Graphic4Mode::addressOf(x, y, false);
	}
	unsigned getAddr() const
	{
		return addr;
	}
	void step(int tx)
	{
		addr += (tx >> 1);
	}

private:
	unsigned addr;
};

struct IncrByteAddr5
{
	IncrByteAddr5(unsigned x, unsigned y, int /*tx*/)
	{
		addr = Graphic5Mode::addressOf(x, y, false);
	}
	unsigned getAddr() const
	{
		return addr;
	}
	void step(int tx)
	{
		addr += (tx >> 2);
	}

private:
	unsigned addr;
};

struct IncrByteAddr7
{
	IncrByteAddr7(unsigned x, unsigned y, int tx)
		: delta2((tx > 0) ? ( 0x10000 ^ (1 - 0x10000))
		                  : (-0x10000 ^ (0x10000 - 1)))
	{
		addr = Graphic7Mode::addressOf(x, y, false);
		delta = (tx > 0) ? 0x10000 : (0x10000 - 1);
		if (x & 1) delta ^= delta2;
	}
	unsigned getAddr() const
	{
		return addr;
	}
	void step(int /*tx*/)
	{
		addr += delta;
		delta ^= delta2;
	}

private:
	unsigned addr;
	unsigned delta;
	const unsigned delta2;
};

struct IncrByteAddr6 : IncrByteAddr7
{
	IncrByteAddr6(unsigned x, unsigned y, int tx)
		: IncrByteAddr7(x >> 1, y, tx)
	{
	}
};

/** Incremental address calculation (pixel-based)
 */
struct IncrPixelAddr4
{
	IncrPixelAddr4(unsigned x, unsigned y, int tx)
	{
		addr = Graphic4Mode::addressOf(x, y, false);
		delta = (tx == 1) ? (x & 1) : ((x & 1) - 1);
	}
	unsigned getAddr() const { return addr; }
	void step(int tx)
	{
		addr += delta;
		delta ^= tx;
	}
private:
	unsigned addr;
	unsigned delta;
};

struct IncrPixelAddr5
{
	IncrPixelAddr5(unsigned x, unsigned y, int tx)
	{
		addr = Graphic5Mode::addressOf(x, y, false);
		                       // x |  0 |  1 |  2 |  3
		                       //-----------------------
		c1 = -(signed(x) & 1); //   |  0 | -1 |  0 | -1
		c2 = (x & 2) >> 1;     //   |  0 |  0 |  1 |  1
		if (tx < 0) {
			c1 = ~c1;      //   | -1 |  0 | -1 |  0
			c2 -= 1;       //   | -1 | -1 |  0 |  0
		}
	}
	unsigned getAddr() const { return addr; }
	void step(int tx)
	{
		addr += (c1 & c2);
		c2 ^= (c1 & tx);
		c1 = ~c1;
	}
private:
	unsigned addr;
	unsigned c1;
	unsigned c2;
};

struct IncrPixelAddr6
{
	IncrPixelAddr6(unsigned x, unsigned y, int tx)
		: c3((tx == 1) ? unsigned(0x10000 ^ (1 - 0x10000))   // == -0x1FFFF
		               : unsigned(-0x10000 ^ (0x10000 - 1))) // == -1
	{
		addr = Graphic6Mode::addressOf(x, y, false);
		c1 = -(signed(x) & 1);
		if (tx == 1) {
			c2 = (x & 2) ? (1 - 0x10000) :  0x10000;
		} else {
			c1 = ~c1;
			c2 = (x & 2) ? -0x10000 : (0x10000 - 1);
		}
	}
	unsigned getAddr() const { return addr; }
	void step(int /*tx*/)
	{
		addr += (c1 & c2);
		c2 ^= (c1 & c3);
		c1 = ~c1;
	}
private:
	unsigned addr;
	unsigned c1;
	unsigned c2;
	const unsigned c3;
};


/** Incremental mask calculation.
 * Mask has 0-bits in the position of the pixel, 1-bits elsewhere.
 */
struct IncrMask4
{
	IncrMask4(unsigned x, int /*tx*/)
	{
		mask = 0x0F << ((x & 1) << 2);
	}
	byte getMask() const
	{
		return mask;
	}
	void step()
	{
		mask = ~mask;
	}
private:
	byte mask;
};

struct IncrMask5
{
	IncrMask5(unsigned x, int tx)
		: shift((tx > 0) ? 6 : 2)
	{
		mask = ~(0xC0 >> ((x & 3) << 1));
	}
	byte getMask() const
	{
		return mask;
	}
	void step()
	{
		mask = (mask << shift) | (mask >> (8 - shift));
	}
private:
	byte mask;
	const byte shift;
};

struct IncrMask7
{
	IncrMask7(unsigned /*x*/, int /*tx*/) {}
	byte getMask() const
	{
		return 0;
	}
	void step() {}
};


/* Shift between source and destination pixel for LMMM command.
 */
struct IncrShift4
{
	IncrShift4(unsigned sx, unsigned dx)
		: shift(((dx - sx) & 1) * 4)
	{
	};
	byte doShift(byte color) const
	{
		return (color >> shift) | (color << shift);
	}
private:
	const byte shift;
};

struct IncrShift5
{
	IncrShift5(unsigned sx, unsigned dx)
		: shift(((dx - sx) & 3) * 2)
	{
	};
	byte doShift(byte color) const
	{
		return (color >> shift) | (color << (8 - shift));
	}
private:
	const byte shift;
};

struct IncrShift7
{
	IncrShift7(unsigned /*sx*/, unsigned /*dx*/) {}
	byte doShift(byte color) const
	{
		return color;
	}
};


// Logical operations:

struct DummyOp {
	void operator()(EmuTime::param /*time*/, VDPVRAM& /*vram*/,
	                unsigned /*addr*/, byte /*color*/, byte /*mask*/) const
	{
		// Undefined logical operations do nothing.
	}
};

struct ImpOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte mask) const
	{
		vram.cmdWrite(addr,
			(vram.cmdWriteWindow.readNP(addr) & mask) | color,
			time);
	}
};

struct AndOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte mask) const
	{
		vram.cmdWrite(addr,
			vram.cmdWriteWindow.readNP(addr) & (color | mask),
			time);
	}
};

struct OrOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte /*mask*/) const
	{
		vram.cmdWrite(addr,
			vram.cmdWriteWindow.readNP(addr) | color,
			time);
	}
};

struct XorOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte /*mask*/)
	{
		vram.cmdWrite(addr,
			vram.cmdWriteWindow.readNP(addr) ^ color,
			time);
	}
};

struct NotOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte mask)
	{
		vram.cmdWrite(addr,
			(vram.cmdWriteWindow.readNP(addr) & mask) | ~(color | mask),
			time);
	}
};

template <typename Op>
struct TransparentOp : public Op {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte color, byte mask)
	{
		if (color) Op::operator()(time, vram, addr, color, mask);
	}
};
typedef TransparentOp<ImpOp> TImpOp;
typedef TransparentOp<AndOp> TAndOp;
typedef TransparentOp<OrOp> TOrOp;
typedef TransparentOp<XorOp> TXorOp;
typedef TransparentOp<NotOp> TNotOp;


// Commands

/** Abort
  */
struct AbortCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};

void AbortCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	engine.commandDone(time);
}

/** Point
  */
template <class Mode> struct PointCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void PointCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.disable(time);
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;

	engine.COL = likely(doPoint)
	           ? Mode::point(vram, engine.SX, engine.SY, srcExt)
	           : 0xFF;
	engine.commandDone(time);
}

/** Pset
  */
template <typename Mode, typename LogOp> struct PsetCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode, typename LogOp>
void PsetCmd<Mode, LogOp>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;

	if (likely(doPset)) {
		byte col = engine.COL & Mode::COLOR_MASK;
		Mode::pset(time, vram, engine.DX, engine.DY,
		           dstExt, col, LogOp());
	}
	engine.commandDone(time);
}

/** Search a dot.
  */
struct SrchBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode> struct SrchCmd : public SrchBaseCmd
{
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

void SrchBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.disable(time);
	engine.ASX = engine.SX;
	engine.statusChangeTime = EmuTime::zero; // we can find it any moment
}

template <typename Mode>
void SrchCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	byte CL = engine.COL & Mode::COLOR_MASK;
	int TX = (engine.ARG & DIX) ? -1 : 1;
	bool AEQ = (engine.ARG & EQ) != 0; // TODO: Do we look for "==" or "!="?
	unsigned delta = SRCH_TIMING[engine.getTiming()];

	// TODO use MXS or MXD here?
	//  datasheet says MXD but MXS seems more logical
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;

	while (engine.clock.before(time)) {
		engine.clock.fastAdd(delta);
		byte p = likely(doPoint)
		       ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
		       : 0xFF;
		if ((p == CL) ^ AEQ) {
			engine.status |= 0x10; // border detected
			engine.commandDone(engine.clock.getTime());
			break;
		}
		if ((engine.ASX += TX) & Mode::PIXELS_PER_LINE) {
			engine.status &= 0xEF; // border not detected
			engine.commandDone(engine.clock.getTime());
			break;
		}
	}
}

/** Draw a line.
  */
struct LineBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LineCmd : public LineBaseCmd
{
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

void LineBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	engine.ASX = ((engine.NX - 1) >> 1);
	engine.ADX = engine.DX;
	engine.ANX = 0;
	engine.statusChangeTime = EmuTime::zero; // TODO can still be optimized
}

template <typename Mode, typename LogOp>
void LineCmd<Mode, LogOp>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	byte CL = engine.COL & Mode::COLOR_MASK;
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	unsigned delta = LINE_TIMING[engine.getTiming()];
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;

	// note: X-axis major and Y-axis major routines can be made more
	// symmetrical by splitting the combined end-test
	if ((engine.ARG & MAJ) == 0) {
		// X-Axis is major direction.
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				Mode::pset(engine.clock.getTime(), vram, engine.ADX, engine.DY,
				           dstExt, CL, LogOp());
			}
			engine.clock.fastAdd(delta);
			engine.ADX += TX;
			// confirmed on real HW:
			//  - end-test happens before DY += TY
			//  - (ADX & PPL) test only happens after first pixel
			//    is drawn. And it does test with 'AND' (not with ==)
			if (engine.ANX++ == engine.NX || (engine.ADX & Mode::PIXELS_PER_LINE)) {
				engine.commandDone(engine.clock.getTime());
				break;
			}
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.DY += TY;
			}
			engine.ASX -= engine.NY;
			engine.ASX &= 1023; // mask to 10 bits range
		}
	} else {
		// Y-Axis is major direction.
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				Mode::pset(engine.clock.getTime(), vram, engine.ADX, engine.DY,
				           dstExt, CL, LogOp());
			}
			engine.clock.fastAdd(delta);
			// confirmed on real HW: DY += TY happens before end-test
			engine.DY += TY;
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.ADX += TX;
			}
			engine.ASX -= engine.NY;
			engine.ASX &= 1023; // mask to 10 bits range
			if (engine.ANX++ == engine.NX || (engine.ADX & Mode::PIXELS_PER_LINE)) {
				engine.commandDone(engine.clock.getTime());
				break;
			}
		}
	}
}

/** Abstract base class for block commands.
  */
class BlockCmd : public VDPCmd
{
protected:
	void calcFinishTime(VDPCmdEngine& engine,
	                    unsigned NX, unsigned NY, const unsigned* timing);
};

void BlockCmd::calcFinishTime(VDPCmdEngine& engine,
                              unsigned NX, unsigned NY, const unsigned* timing)
{
	if (engine.currentCommand) {
		unsigned ticks = ((NX * (NY - 1)) + engine.ANX) *
		                 timing[engine.getTiming()];
		engine.statusChangeTime = engine.clock + ticks;
	}
}

/** Logical move VDP -> VRAM.
  */
template <typename Mode> struct LmmvBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmvCmd : public LmmvBaseCmd<Mode>
{
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmvBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	calcFinishTime(engine, NX, NY, LMMV_TIMING);
}

template <typename Mode, typename LogOp>
void LmmvCmd<Mode, LogOp>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_pixel<Mode>(engine.ADX, engine.ANX, engine.ARG);
	byte CL = engine.COL & Mode::COLOR_MASK;
	unsigned delta = LMMV_TIMING[engine.getTiming()];
	bool dstExt = (engine.ARG & MXD) != 0;

	if (unlikely(dstExt)) {
		bool doPset = !dstExt || engine.hasExtendedVRAM;
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				Mode::pset(engine.clock.getTime(), vram, engine.ADX, engine.DY,
					   dstExt, CL, LogOp());
			}
			engine.clock.fastAdd(delta);
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		CL = Mode::duplicate(CL);
		while (engine.clock.before(time)) {
			typename Mode::IncrPixelAddr dstAddr(engine.ADX, engine.DY, TX);
			typename Mode::IncrMask      dstMask(engine.ADX, TX);
			unsigned ticks = engine.clock.getTicksTillUp(time);
			unsigned num = delta
			             ? std::min((ticks + delta - 1) / delta, engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte mask = dstMask.getMask();
				psetFast(engine.clock.getTime(), vram, dstAddr.getAddr(),
					 CL & ~mask, mask, LogOp());
				engine.clock.fastAdd(delta);
				dstAddr.step(TX);
				dstMask.step();
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.DY += TY;
				engine.NY -= 1;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(!engine.clock.before(time));
				break;
			}
		}
	}

	this->calcFinishTime(engine, NX, NY, LMMV_TIMING);
}

/** Logical move VRAM -> VRAM.
  */
template <typename Mode> struct LmmmBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmmCmd : public LmmmBaseCmd<Mode>
{
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmmBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_2_pixel<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ASX = engine.SX;
	engine.ADX = engine.DX;
	engine.ANX = NX;
	calcFinishTime(engine, NX, NY, LMMM_TIMING);
}

template <typename Mode, typename LogOp>
void LmmmCmd<Mode, LogOp>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_2_pixel<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_2_pixel<Mode>(engine.ASX, engine.ADX, engine.ANX, engine.ARG);
	unsigned delta = LMMM_TIMING[engine.getTiming()];
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool dstExt  = (engine.ARG & MXD) != 0;

	if (unlikely(srcExt) || unlikely(dstExt)) {
		bool doPoint = !srcExt || engine.hasExtendedVRAM;
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				byte p = likely(doPoint)
				       ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
				       : 0xFF;
				Mode::pset(engine.clock.getTime(), vram, engine.ADX, engine.DY,
					   dstExt, p, LogOp());
			}
			engine.clock.fastAdd(delta);
			engine.ASX += TX; engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.clock.before(time)) {
			typename Mode::IncrPixelAddr srcAddr(engine.ASX, engine.SY, TX);
			typename Mode::IncrPixelAddr dstAddr(engine.ADX, engine.DY, TX);
			typename Mode::IncrMask      dstMask(engine.ADX,            TX);
			typename Mode::IncrShift     shift  (engine.ASX, engine.ADX);
			unsigned ticks = engine.clock.getTicksTillUp(time);
			unsigned num = delta
			             ? std::min((ticks + delta - 1) / delta, engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				p = shift.doShift(p);
				byte mask = dstMask.getMask();
				psetFast(engine.clock.getTime(), vram, dstAddr.getAddr(),
					 p & ~mask, mask, LogOp());
				engine.clock.fastAdd(delta);
				srcAddr.step(TX);
				dstAddr.step(TX);
				dstMask.step();
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.SY += TY;
				engine.DY += TY;
				engine.NY -= 1;
				engine.ASX = engine.SX;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			} else {
				engine.ASX += num * TX;
				engine.ADX += num * TX;
				assert(!engine.clock.before(time));
				break;
			}
		}
	}

	this->calcFinishTime(engine, NX, NY, LMMM_TIMING);
}

/** Logical move VRAM -> CPU.
  */
template <typename Mode> struct LmcmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void LmcmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.disable(time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.SX, engine.NX, engine.ARG);
	engine.ASX = engine.SX;
	engine.ANX = NX;
	engine.statusChangeTime = EmuTime::zero;
	engine.transfer = true;
	engine.status |= 0x80;
}

template <typename Mode>
void LmcmCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.SX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.SY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_pixel<Mode>(engine.ASX, engine.ANX, engine.ARG);
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;

	if (engine.transfer) {
		engine.COL = likely(doPoint)
		           ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
		           : 0xFF;
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += LMMV_TIMING[engine.getTiming()];
		engine.transfer = false;
		engine.ASX += TX; --engine.ANX;
		if (engine.ANX == 0) {
			engine.SY += TY; --(engine.NY);
			engine.ASX = engine.SX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(time);
			}
		}
	}
}

/** Logical move CPU -> VRAM.
  */
template <typename Mode> struct LmmcBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmcCmd : public LmmcBaseCmd<Mode>
{
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmcBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.statusChangeTime = EmuTime::zero;
	engine.transfer = true;
	engine.status |= 0x80;
}

template <typename Mode, typename LogOp>
void LmmcCmd<Mode, LogOp>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_pixel<Mode>(engine.ADX, engine.ANX, engine.ARG);
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset  = !dstExt || engine.hasExtendedVRAM;

	if (engine.transfer) {
		byte col = engine.COL & Mode::COLOR_MASK;
		// TODO: Write time is inaccurate.
		engine.clock.reset(time);
		if (likely(doPset)) {
			Mode::pset(engine.clock.getTime(), vram, engine.ADX, engine.DY,
			           dstExt, col, LogOp());
		}
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += LMMV_TIMING[engine.getTiming()];
		engine.transfer = false;

		engine.ADX += TX; --engine.ANX;
		if (engine.ANX == 0) {
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(time);
			}
		}
	}
}

/** High-speed move VDP -> VRAM.
  */
template <typename Mode> struct HmmvCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmvCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	calcFinishTime(engine, NX, NY, HMMV_TIMING);
}

template <typename Mode>
void HmmvCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_byte<Mode>(
		engine.ADX, engine.ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine.ARG );
	unsigned delta = HMMV_TIMING[engine.getTiming()];
	bool dstExt = (engine.ARG & MXD) != 0;

	if (unlikely(dstExt)) {
		bool doPset = !dstExt || engine.hasExtendedVRAM;
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      engine.COL, engine.clock.getTime());
			}
			engine.clock.fastAdd(delta);
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.clock.before(time)) {
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			unsigned ticks = engine.clock.getTicksTillUp(time);
			unsigned num = delta
			             ? std::min((ticks + delta - 1) / delta, engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				vram.cmdWrite(dstAddr.getAddr(), engine.COL,
					      engine.clock.getTime());
				engine.clock.fastAdd(delta);
				dstAddr.step(TX);
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.DY += TY;
				engine.NY -= 1;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(!engine.clock.before(time));
				break;
			}
		}
	}

	calcFinishTime(engine, NX, NY, HMMV_TIMING);
}

/** High-speed move VRAM -> VRAM.
  */
template <typename Mode> struct HmmmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_2_byte<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ASX = engine.SX;
	engine.ADX = engine.DX;
	engine.ANX = NX;
	calcFinishTime(engine, NX, NY, HMMM_TIMING);
}

template <typename Mode>
void HmmmCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_2_byte<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_2_byte<Mode>(
		engine.ASX, engine.ADX, engine.ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine.ARG );
	unsigned delta = HMMM_TIMING[engine.getTiming()];
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool dstExt  = (engine.ARG & MXD) != 0;

	if (unlikely(srcExt || dstExt)) {
		bool doPoint = !srcExt || engine.hasExtendedVRAM;
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				byte p = likely(doPoint)
				       ? vram.cmdReadWindow.readNP(
					       Mode::addressOf(engine.ASX, engine.SY, srcExt))
				       : 0xFF;
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      p, engine.clock.getTime());
			}
			engine.clock.fastAdd(delta);
			engine.ASX += TX; engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.clock.before(time)) {
			typename Mode::IncrByteAddr srcAddr(engine.ASX, engine.SY, TX);
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			unsigned ticks = engine.clock.getTicksTillUp(time);
			unsigned num = delta
			             ? std::min((ticks + delta - 1) / delta, engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engine.clock.getTime());
				engine.clock.fastAdd(delta);
				srcAddr.step(TX);
				dstAddr.step(TX);
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.SY += TY;
				engine.DY += TY;
				engine.NY -= 1;
				engine.ASX = engine.SX;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			} else {
				engine.ASX += num * TX;
				engine.ADX += num * TX;
				assert(!engine.clock.before(time));
				break;
			}
		}
	}

	calcFinishTime(engine, NX, NY, HMMM_TIMING);
}

/** High-speed move VRAM -> VRAM (Y direction only).
  */
template <typename Mode> struct YmmmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void YmmmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.setMask(0x3FFFF, -1 << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, 512, engine.ARG);
		// large enough so that it gets clipped
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	calcFinishTime(engine, NX, NY, YMMM_TIMING);
}

template <typename Mode>
void YmmmCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, 512, engine.ARG);
		// large enough so that it gets clipped
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_byte<Mode>(engine.ADX, 512, engine.ARG);
	unsigned delta = YMMM_TIMING[engine.getTiming()];

	// TODO does this use MXD for both read and write?
	//  it says so in the datasheet, but it seems unlogical
	//  OTOH YMMM also uses DX for both read and write
	bool dstExt = (engine.ARG & MXD) != 0;

	if (unlikely(dstExt)) {
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.clock.before(time)) {
			if (likely(doPset)) {
				byte p = vram.cmdReadWindow.readNP(
					      Mode::addressOf(engine.ADX, engine.SY, dstExt));
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      p, engine.clock.getTime());
			}
			engine.clock.fastAdd(delta);
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.clock.before(time)) {
			typename Mode::IncrByteAddr srcAddr(engine.ADX, engine.SY, TX);
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			unsigned ticks = engine.clock.getTicksTillUp(time);
			unsigned num = delta
			             ? std::min((ticks + delta - 1) / delta, engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engine.clock.getTime());
				engine.clock.fastAdd(delta);
				srcAddr.step(TX);
				dstAddr.step(TX);
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.SY += TY;
				engine.DY += TY;
				engine.NY -= 1;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.clock.getTime());
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(!engine.clock.before(time));
				break;
			}
		}
	}

	calcFinishTime(engine, NX, NY, YMMM_TIMING);
}

/** High-speed move CPU -> VRAM.
  */
template <typename Mode> struct HmmcCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param time, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmcCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.clock.reset(time);
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1 << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.statusChangeTime = EmuTime::zero;
	engine.transfer = true;
	engine.status |= 0x80;
}

template <typename Mode>
void HmmcCmd<Mode>::execute(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_byte<Mode>(
		engine.ADX, engine.ANX << Mode::PIXELS_PER_BYTE_SHIFT, engine.ARG );
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;

	if (engine.transfer) {
		// TODO: Write time is inaccurate.
		if (likely(doPset)) {
			vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
			              engine.COL, time);
		}
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		//currentTime += HMMV_TIMING[engine.getTiming()];
		engine.transfer = false;

		engine.ADX += TX; --engine.ANX;
		if (engine.ANX == 0) {
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(engine.clock.getTime());
			}
		}
	}
}


// Construction and destruction:

template <template <typename Mode> class Command>
void VDPCmdEngine::createHEngines(unsigned cmd)
{
	commands[cmd + 0][0] = new Command<Graphic4Mode>();
	commands[cmd + 0][1] = new Command<Graphic5Mode>();
	commands[cmd + 0][2] = new Command<Graphic6Mode>();
	commands[cmd + 0][3] = new Command<Graphic7Mode>();
	for (int i = 1; i < 16; ++i) {
		for (int j = 0; j < 4; ++j) {
			commands[cmd + i][j] = commands[cmd + 0][j];
		}
	}
}
void VDPCmdEngine::deleteHEngines(unsigned cmd)
{
	for (int j = 0; j < 4; ++j) {
		delete commands[cmd + 0][j];
	}
}

template <template <typename Mode, typename LopOp> class Command>
void VDPCmdEngine::createLEngines(unsigned cmd, VDPCmd* dummy)
{
	commands[cmd +  0][0] = new Command<Graphic4Mode, ImpOp  >();
	commands[cmd +  0][1] = new Command<Graphic5Mode, ImpOp  >();
	commands[cmd +  0][2] = new Command<Graphic6Mode, ImpOp  >();
	commands[cmd +  0][3] = new Command<Graphic7Mode, ImpOp  >();

	commands[cmd +  1][0] = new Command<Graphic4Mode, AndOp  >();
	commands[cmd +  1][1] = new Command<Graphic5Mode, AndOp  >();
	commands[cmd +  1][2] = new Command<Graphic6Mode, AndOp  >();
	commands[cmd +  1][3] = new Command<Graphic7Mode, AndOp  >();

	commands[cmd +  2][0] = new Command<Graphic4Mode, OrOp   >();
	commands[cmd +  2][1] = new Command<Graphic5Mode, OrOp   >();
	commands[cmd +  2][2] = new Command<Graphic6Mode, OrOp   >();
	commands[cmd +  2][3] = new Command<Graphic7Mode, OrOp   >();

	commands[cmd +  3][0] = new Command<Graphic4Mode, XorOp  >();
	commands[cmd +  3][1] = new Command<Graphic5Mode, XorOp  >();
	commands[cmd +  3][2] = new Command<Graphic6Mode, XorOp  >();
	commands[cmd +  3][3] = new Command<Graphic7Mode, XorOp  >();

	commands[cmd +  4][0] = new Command<Graphic4Mode, NotOp  >();
	commands[cmd +  4][1] = new Command<Graphic5Mode, NotOp  >();
	commands[cmd +  4][2] = new Command<Graphic6Mode, NotOp  >();
	commands[cmd +  4][3] = new Command<Graphic7Mode, NotOp  >();

	commands[cmd +  5][0] = dummy;
	commands[cmd +  5][1] = dummy;
	commands[cmd +  5][2] = dummy;
	commands[cmd +  5][3] = dummy;

	commands[cmd +  6][0] = dummy;
	commands[cmd +  6][1] = dummy;
	commands[cmd +  6][2] = dummy;
	commands[cmd +  6][3] = dummy;

	commands[cmd +  7][0] = dummy;
	commands[cmd +  7][1] = dummy;
	commands[cmd +  7][2] = dummy;
	commands[cmd +  7][3] = dummy;

	commands[cmd +  8][0] = new Command<Graphic4Mode, TImpOp >();
	commands[cmd +  8][1] = new Command<Graphic5Mode, TImpOp >();
	commands[cmd +  8][2] = new Command<Graphic6Mode, TImpOp >();
	commands[cmd +  8][3] = new Command<Graphic7Mode, TImpOp >();

	commands[cmd +  9][0] = new Command<Graphic4Mode, TAndOp >();
	commands[cmd +  9][1] = new Command<Graphic5Mode, TAndOp >();
	commands[cmd +  9][2] = new Command<Graphic6Mode, TAndOp >();
	commands[cmd +  9][3] = new Command<Graphic7Mode, TAndOp >();

	commands[cmd + 10][0] = new Command<Graphic4Mode, TOrOp  >();
	commands[cmd + 10][1] = new Command<Graphic5Mode, TOrOp  >();
	commands[cmd + 10][2] = new Command<Graphic6Mode, TOrOp  >();
	commands[cmd + 10][3] = new Command<Graphic7Mode, TOrOp  >();

	commands[cmd + 11][0] = new Command<Graphic4Mode, TXorOp >();
	commands[cmd + 11][1] = new Command<Graphic5Mode, TXorOp >();
	commands[cmd + 11][2] = new Command<Graphic6Mode, TXorOp >();
	commands[cmd + 11][3] = new Command<Graphic7Mode, TXorOp >();

	commands[cmd + 12][0] = new Command<Graphic4Mode, TNotOp >();
	commands[cmd + 12][1] = new Command<Graphic5Mode, TNotOp >();
	commands[cmd + 12][2] = new Command<Graphic6Mode, TNotOp >();
	commands[cmd + 12][3] = new Command<Graphic7Mode, TNotOp >();

	commands[cmd + 13][0] = dummy;
	commands[cmd + 13][1] = dummy;
	commands[cmd + 13][2] = dummy;
	commands[cmd + 13][3] = dummy;

	commands[cmd + 14][0] = dummy;
	commands[cmd + 14][1] = dummy;
	commands[cmd + 14][2] = dummy;
	commands[cmd + 14][3] = dummy;

	commands[cmd + 15][0] = dummy;
	commands[cmd + 15][1] = dummy;
	commands[cmd + 15][2] = dummy;
	commands[cmd + 15][3] = dummy;
}

void VDPCmdEngine::deleteLEngines(unsigned cmd)
{
	for (int i = 0; i < 5; ++i) {
		for (int j = 0; j < 4; ++j) {
			delete commands[cmd + i + 0][j];
			delete commands[cmd + i + 8][j];
		}
	}
}


VDPCmdEngine::VDPCmdEngine(VDP& vdp_, RenderSettings& renderSettings_,
	CommandController& commandController)
	: vdp(vdp_), vram(vdp.getVRAM())
	, renderSettings(renderSettings_)
	, cmdTraceSetting(new BooleanSetting(
		commandController, "vdpcmdtrace",
		"VDP command tracing on/off", false))
	, clock(EmuTime::zero)
	, statusChangeTime(EmuTime::infinity)
	, hasExtendedVRAM(vram.getSize() == (192 * 1024))
{
	status = 0;
	transfer = false;
	SX = SY = DX = DY = NX = NY = 0;
	ASX = ADX = ANX = 0;
	COL = ARG = CMD = 0;

	AbortCmd* abort = new AbortCmd();
	VDPCmd* dummy = new PsetCmd<Graphic7Mode, DummyOp>();
	for (unsigned cmd = 0x0; cmd < 0x40; ++cmd) {
		for (unsigned mode = 0; mode < 4; ++mode) {
			commands[cmd][mode] = abort;
		}
	}
	createHEngines<PointCmd>(0x40);
	createLEngines<PsetCmd >(0x50, dummy);
	createHEngines<SrchCmd >(0x60);
	createLEngines<LineCmd >(0x70, dummy);
	createLEngines<LmmvCmd >(0x80, dummy);
	createLEngines<LmmmCmd >(0x90, dummy);
	createHEngines<LmcmCmd >(0xA0);
	createLEngines<LmmcCmd >(0xB0, dummy);
	createHEngines<HmmvCmd >(0xC0);
	createHEngines<HmmmCmd >(0xD0);
	createHEngines<YmmmCmd >(0xE0);
	createHEngines<HmmcCmd >(0xF0);
	currentCommand = NULL;

	brokenTiming = renderSettings.getCmdTiming().getValue();

	renderSettings.getCmdTiming().attach(*this);
}

VDPCmdEngine::~VDPCmdEngine()
{
	renderSettings.getCmdTiming().detach(*this);

	delete commands[0x00][0]; // abort command
	delete commands[0x55][0]; // dummy command
	deleteHEngines(0x40);
	deleteLEngines(0x50);
	deleteHEngines(0x60);
	deleteLEngines(0x70);
	deleteLEngines(0x80);
	deleteLEngines(0x90);
	deleteHEngines(0xA0);
	deleteLEngines(0xB0);
	deleteHEngines(0xC0);
	deleteHEngines(0xD0);
	deleteHEngines(0xE0);
	deleteHEngines(0xF0);
}

void VDPCmdEngine::reset(EmuTime::param time)
{
	status = 0;
	scrMode = -1;
	for (unsigned i = 0; i < 15; ++i) {
		setCmdReg(i, 0, time);
	}

	updateDisplayMode(vdp.getDisplayMode(), time);
}

void VDPCmdEngine::update(const Setting& setting)
{
	brokenTiming = static_cast<const EnumSetting<bool>*>(&setting)->getValue();
}

void VDPCmdEngine::setCmdReg(byte index, byte value, EmuTime::param time)
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

	case 0x0C: // color
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
		CMD = value;
		executeCommand(time);
		break;
	default:
		UNREACHABLE;
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
	case 0x0E: return CMD;
	default: UNREACHABLE; return 0;
	}
}

void VDPCmdEngine::updateDisplayMode(DisplayMode mode, EmuTime::param time)
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
		if (vdp.getCmdBit()) {
			newScrMode = 3; // like GRAPHIC7
			                // TODO timing might be different
		} else {
			newScrMode = -1; // no commands
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
				currentCommand = commands[CMD][newScrMode];
			}
		}
		sync(time);
		scrMode = newScrMode;
	}
}

void VDPCmdEngine::executeCommand(EmuTime::param time)
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
	currentCommand->start(time, *this);

	// Finish command now if instantaneous command timing is active.
	// Abort finishes on start, so currentCommand can be NULL.
	if (brokenTiming && currentCommand) {
		currentCommand->execute(time, *this);
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

	std::cerr << "VDPCmd " << COMMANDS[CMD >> 4] << '-' << OPS[CMD & 15]
		<<  '(' << int(SX) << ',' << int(SY) << ")->("
		        << int(DX) << ',' << int(DY) << ")," << int(COL)
		<< " [" << int((ARG & DIX) ? -signed(NX) : NX)
		<<  ',' << int((ARG & DIY) ? -signed(NY) : NY) << ']' << std::endl;
}

void VDPCmdEngine::commandDone(EmuTime::param time)
{
	// Note: TR is not reset yet; it is reset when S#2 is read next.
	status &= 0xFE; // reset CE
	CMD = 0;
	currentCommand = NULL;
	statusChangeTime = EmuTime::infinity;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.disable(time);
}



template<typename Archive>
void VDPCmdEngine::serialize(Archive& ar, unsigned /*version*/)
{
	// In older (development) versions CMD was split in a CMD and a LOG
	// member, though it was combined for the savestate. Only the CMD part
	// was guaranteed to be zero when no command was executing. So when
	// loading an older savestate this can still be the case.
	if (currentCommand == NULL) {
		assert((CMD & 0xF0) == 0); // assert(CMD == 0);
	}

	ar.serialize("clock", clock);
	ar.serialize("statusChangeTime", statusChangeTime);
	ar.serialize("scrMode", scrMode);
	ar.serialize("status", status);
	ar.serialize("transfer", transfer);
	ar.serialize("SX", SX);
	ar.serialize("SY", SY);
	ar.serialize("DX", DX);
	ar.serialize("DY", DY);
	ar.serialize("NX", NX);
	ar.serialize("NY", NY);
	ar.serialize("ASX", ASX);
	ar.serialize("ADX", ADX);
	ar.serialize("ANX", ANX);
	ar.serialize("COL", COL);
	ar.serialize("ARG", ARG);
	ar.serialize("CMD", CMD);

	if (ar.isLoader()) {
		if (CMD & 0xF0) {
			assert(scrMode >= 0);
			currentCommand = commands[CMD][scrMode];
		} else {
			currentCommand = NULL;
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(VDPCmdEngine);

} // namespace openmsx
