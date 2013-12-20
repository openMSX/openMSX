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
#include "TclCallback.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <iostream>
#include <cassert>
#include <algorithm>

using std::min;
using std::max;

namespace openmsx {

using namespace VDPAccessSlots;

// Constants:
const byte MXD = 0x20;
const byte MXS = 0x10;
const byte DIY = 0x08;
const byte DIX = 0x04;
const byte EQ  = 0x02;
const byte MAJ = 0x01;

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
	byte src = vram.cmdWriteWindow.readNP(addr);
	op(time, vram, addr, src, color, mask);
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
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
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
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	byte sh = ((~x) & 1) << 2;
	op(time, vram, addr, src, color << sh, ~(15 << sh));
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
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
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
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	byte sh = ((~x) & 3) << 1;
	op(time, vram, addr, src, color << sh, ~(3 << sh));
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
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
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
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	byte sh = ((~x) & 1) << 2;
	op(time, vram, addr, src, color << sh, ~(15 << sh));
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
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
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
	EmuTime::param time, VDPVRAM& vram, unsigned /*x*/, unsigned addr,
	byte src, byte color, LogOp op)
{
	op(time, vram, addr, src, color, 0);
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
	void operator()(EmuTime::param /*time*/, VDPVRAM& /*vram*/, unsigned /*addr*/,
	                byte /*src*/, byte /*color*/, byte /*mask*/) const
	{
		// Undefined logical operations do nothing.
	}
};

struct ImpOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte mask) const
	{
		vram.cmdWrite(addr, (src & mask) | color, time);
	}
};

struct AndOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte mask) const
	{
		vram.cmdWrite(addr, src & (color | mask), time);
	}
};

struct OrOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte /*mask*/) const
	{
		vram.cmdWrite(addr, src | color, time);
	}
};

struct XorOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte /*mask*/) const
	{
		vram.cmdWrite(addr, src ^ color, time);
	}
};

struct NotOp {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte mask) const
	{
		vram.cmdWrite(addr, (src & mask) | ~(color | mask), time);
	}
};

template <typename Op>
struct TransparentOp : Op {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte mask) const
	{
		// TODO does this skip the write or re-write the original value
		//      might make a difference in case the CPU has written
		//      the same address inbetween the command read and write
		if (color) Op::operator()(time, vram, addr, src, color, mask);
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
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

void AbortCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	engine.commandDone(time);
}
void AbortCmd::execute(EmuTime::param /*limit*/, VDPCmdEngine& /*engine*/)
{
	UNREACHABLE;
}

/** Point
  */
struct PointBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template<typename Mode> struct PointCmd : public PointBaseCmd
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

void PointBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.disable(time);
	engine.time = time; engine.nextAccessSlot();
	engine.statusChangeTime = EmuTime::zero; // will finish soon
}

template<typename Mode>
void PointCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	if (unlikely(engine.time >= limit)) return;

	VDPVRAM& vram = engine.vram;
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;
	engine.COL = likely(doPoint)
	           ? Mode::point(vram, engine.SX, engine.SY, srcExt)
	           : 0xFF;
	engine.commandDone(engine.time);
}

/** Pset
  */
struct PsetBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template<typename Mode, typename LogOp> struct PsetCmd : public PsetBaseCmd
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

void PsetBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.time = time; engine.nextAccessSlot();
	engine.statusChangeTime = EmuTime::zero; // will finish soon
	engine.phase = 0;
}

template<typename Mode, typename LogOp>
void PsetCmd<Mode, LogOp>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;
	unsigned addr = Mode::addressOf(engine.DX, engine.DY, dstExt);

	switch (engine.phase) {
	case 0:
		if (unlikely(engine.time >= limit)) { engine.phase = 0; break; }
		if (likely(doPset)) {
			engine.tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		engine.nextAccessSlot(DELTA_24); // TODO
		// fall-through
	case 1:
		if (unlikely(engine.time >= limit)) { engine.phase = 1; break; }
		if (likely(doPset)) {
			byte col = engine.COL & Mode::COLOR_MASK;
			Mode::pset(engine.time, vram, engine.DX, addr,
			           engine.tmpDst, col, LogOp());
		}
		engine.commandDone(engine.time);
		break;
	default:
		UNREACHABLE;
	}
}

/** Search a dot.
  */
struct SrchBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode> struct SrchCmd : public SrchBaseCmd
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

void SrchBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.disable(time);
	engine.ASX = engine.SX;
	engine.time = time; engine.nextAccessSlot();
	engine.statusChangeTime = EmuTime::zero; // we can find it any moment
}

template <typename Mode>
void SrchCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	byte CL = engine.COL & Mode::COLOR_MASK;
	int TX = (engine.ARG & DIX) ? -1 : 1;
	bool AEQ = (engine.ARG & EQ) != 0; // TODO: Do we look for "==" or "!="?

	// TODO use MXS or MXD here?
	//  datasheet says MXD but MXS seems more logical
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;
	auto calculator = engine.getSlotCalculator(limit);

	while (!calculator.limitReached()) {
		byte p = likely(doPoint)
		       ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
		       : 0xFF;
		if ((p == CL) ^ AEQ) {
			engine.status |= 0x10; // border detected
			engine.commandDone(calculator.getTime());
			break;
		}
		if ((engine.ASX += TX) & Mode::PIXELS_PER_LINE) {
			engine.status &= 0xEF; // border not detected
			engine.commandDone(calculator.getTime());
			break;
		}
		calculator.next(DELTA_88); // TODO
	}
	engine.time = calculator.getTime();
}

/** Draw a line.
  */
struct LineBaseCmd : public VDPCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LineCmd : public LineBaseCmd
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

void LineBaseCmd::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	engine.ASX = ((engine.NX - 1) >> 1);
	engine.ADX = engine.DX;
	engine.ANX = 0;
	engine.time = time; engine.nextAccessSlot();
	engine.statusChangeTime = EmuTime::zero; // TODO can still be optimized
	engine.phase = 0;
}

template <typename Mode, typename LogOp>
void LineCmd<Mode, LogOp>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	// See doc/line-speed.txt for some background info on the timing.
	VDPVRAM& vram = engine.vram;
	byte CL = engine.COL & Mode::COLOR_MASK;
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;
	unsigned addr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
	auto calculator = engine.getSlotCalculator(limit);

	switch (engine.phase) {
	case 0:
loop:		if (unlikely(calculator.limitReached())) { engine.phase = 0; break; }
		if (likely(doPset)) {
			engine.tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		calculator.next(DELTA_24);
		// fall-through
	case 1: {
		if (unlikely(calculator.limitReached())) { engine.phase = 1; break; }
		if (likely(doPset)) {
			Mode::pset(calculator.getTime(), vram, engine.ADX, addr,
			           engine.tmpDst, CL, LogOp());
		}

		Delta delta = DELTA_88;
		if ((engine.ARG & MAJ) == 0) {
			// X-Axis is major direction.
			engine.ADX += TX;
			// confirmed on real HW:
			//  - end-test happens before DY += TY
			//  - (ADX & PPL) test only happens after first pixel
			//    is drawn. And it does test with 'AND' (not with ==)
			if (engine.ANX++ == engine.NX || (engine.ADX & Mode::PIXELS_PER_LINE)) {
				engine.commandDone(calculator.getTime());
				break;
			}
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.DY += TY;
				delta = DELTA_120; // 88 + 32
			}
			engine.ASX -= engine.NY;
			engine.ASX &= 1023; // mask to 10 bits range
		} else {
			// Y-Axis is major direction.
			// confirmed on real HW: DY += TY happens before end-test
			engine.DY += TY;
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.ADX += TX;
				delta = DELTA_120; // 88 + 32
			}
			engine.ASX -= engine.NY;
			engine.ASX &= 1023; // mask to 10 bits range
			if (engine.ANX++ == engine.NX || (engine.ADX & Mode::PIXELS_PER_LINE)) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		addr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engine.time = calculator.getTime();
}

/** Abstract base class for block commands.
  */
class BlockCmd : public VDPCmd
{
protected:
	void calcFinishTime(VDPCmdEngine& engine,
	                    unsigned NX, unsigned NY, unsigned ticksPerPixel);
};

void BlockCmd::calcFinishTime(VDPCmdEngine& engine,
                              unsigned NX, unsigned NY, unsigned ticksPerPixel)
{
	if (!engine.currentCommand) return;

	// Underestimation for when the command will be finished. This assumes
	// we never have to wait for access slots and that there's no overhead
	// per line.
	auto t = VDP::VDPClock::duration(ticksPerPixel);
	t *= ((NX * (NY - 1)) + engine.ANX);
	engine.statusChangeTime = engine.time + t;
}

/** Logical move VDP -> VRAM.
  */
template <typename Mode> struct LmmvBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmvCmd : public LmmvBaseCmd<Mode>
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmvBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.time = time; engine.nextAccessSlot();
	calcFinishTime(engine, NX, NY, 72 + 24);
	engine.phase = 0;
}

template <typename Mode, typename LogOp>
void LmmvCmd<Mode, LogOp>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_pixel<Mode>(engine.ADX, engine.ANX, engine.ARG);
	byte CL = engine.COL & Mode::COLOR_MASK;
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset = !dstExt || engine.hasExtendedVRAM;
	unsigned addr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
	auto calculator = engine.getSlotCalculator(limit);

	switch (engine.phase) {
	case 0:
loop:		if (unlikely(calculator.limitReached())) { engine.phase = 0; break; }
		if (likely(doPset)) {
			engine.tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		calculator.next(DELTA_24);
		// fall-through
	case 1: {
		if (unlikely(calculator.limitReached())) { engine.phase = 1; break; }
		if (likely(doPset)) {
			Mode::pset(calculator.getTime(), vram, engine.ADX, addr,
			           engine.tmpDst, CL, LogOp());
		}
		engine.ADX += TX;
		Delta delta = DELTA_72;
		if (--engine.ANX == 0) {
			delta = DELTA_136; // 72 + 64;
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		addr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engine.time = calculator.getTime();
	this->calcFinishTime(engine, NX, NY, 72 + 24);

	/*
	if (unlikely(dstExt)) {
		bool doPset = !dstExt || engine.hasExtendedVRAM;
		while (engine.time < limit) {
			if (likely(doPset)) {
				Mode::pset(engine.time, vram, engine.ADX, engine.DY,
					   dstExt, CL, LogOp());
			}
			engine.time += delta;
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		CL = Mode::duplicate(CL);
		while (engine.time < limit) {
			typename Mode::IncrPixelAddr dstAddr(engine.ADX, engine.DY, TX);
			typename Mode::IncrMask      dstMask(engine.ADX, TX);
			EmuDuration dur = time - engine.time;
			unsigned num = (delta != EmuDuration::zero)
			             ? std::min(dur.divUp(delta), engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte mask = dstMask.getMask();
				psetFast(engine.time, vram, dstAddr.getAddr(),
					 CL & ~mask, mask, LogOp());
				engine.time += delta;
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
					engine.commandDone(engine.time);
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(engine.time >= limit);
				break;
			}
		}
	}
	*/
}

/** Logical move VRAM -> VRAM.
  */
template <typename Mode> struct LmmmBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmmCmd : public LmmmBaseCmd<Mode>
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmmBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_2_pixel<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ASX = engine.SX;
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.time = time; engine.nextAccessSlot();
	calcFinishTime(engine, NX, NY, 64 + 32 + 24);
	engine.phase = 0;
}

template <typename Mode, typename LogOp>
void LmmmCmd<Mode, LogOp>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_2_pixel<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_2_pixel<Mode>(engine.ASX, engine.ADX, engine.ANX, engine.ARG);
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool dstExt  = (engine.ARG & MXD) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;
	bool doPset  = !dstExt || engine.hasExtendedVRAM;
	unsigned dstAddr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
	auto calculator = engine.getSlotCalculator(limit);

	switch (engine.phase) {
	case 0:
loop:		if (unlikely(calculator.limitReached())) { engine.phase = 0; break; }
		engine.tmpSrc = likely(doPoint)
			? Mode::point(vram, engine.ASX, engine.SY, srcExt)
			: 0xFF;
		calculator.next(DELTA_32);
		// fall-through
	case 1:
		if (unlikely(calculator.limitReached())) { engine.phase = 1; break; }
		if (likely(doPset)) {
			engine.tmpDst = vram.cmdWriteWindow.readNP(dstAddr);
		}
		calculator.next(DELTA_24);
		// fall-through
	case 2: {
		if (unlikely(calculator.limitReached())) { engine.phase = 2; break; }
		if (likely(doPset)) {
			Mode::pset(calculator.getTime(), vram, engine.ADX, dstAddr,
			           engine.tmpDst, engine.tmpSrc, LogOp());
		}
		engine.ASX += TX; engine.ADX += TX;
		Delta delta = DELTA_64;
		if (--engine.ANX == 0) {
			delta = DELTA_128; // 64 + 64
			engine.SY += TY; engine.DY += TY; --(engine.NY);
			engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		dstAddr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engine.time = calculator.getTime();
	this->calcFinishTime(engine, NX, NY, 64 + 32 + 24);

	/*if (unlikely(srcExt) || unlikely(dstExt)) {
		bool doPoint = !srcExt || engine.hasExtendedVRAM;
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.time < limit) {
			if (likely(doPset)) {
				byte p = likely(doPoint)
				       ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
				       : 0xFF;
				Mode::pset(engine.time, vram, engine.ADX, engine.DY,
					   dstExt, p, LogOp());
			}
			engine.time += delta;
			engine.ASX += TX; engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.time < limit) {
			typename Mode::IncrPixelAddr srcAddr(engine.ASX, engine.SY, TX);
			typename Mode::IncrPixelAddr dstAddr(engine.ADX, engine.DY, TX);
			typename Mode::IncrMask      dstMask(engine.ADX,            TX);
			typename Mode::IncrShift     shift  (engine.ASX, engine.ADX);
			EmuDuration dur = limit - engine.time;
			unsigned num = (delta != EmuDuration::zero)
			             ? std::min(dur.divUp(delta), engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				p = shift.doShift(p);
				byte mask = dstMask.getMask();
				psetFast(engine.time, vram, dstAddr.getAddr(),
					 p & ~mask, mask, LogOp());
				engine.time += delta;
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
					engine.commandDone(engine.time);
					break;
				}
			} else {
				engine.ASX += num * TX;
				engine.ADX += num * TX;
				assert(engine.time >= limit);
				break;
			}
		}
	}
	*/
}

/** Logical move VRAM -> CPU.
  */
template <typename Mode> struct LmcmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void LmcmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.disable(time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.SX, engine.NX, engine.ARG);
	engine.ASX = engine.SX;
	engine.ANX = NX;
	engine.transfer = true;
	engine.status |= 0x80;
	engine.time = time; engine.nextAccessSlot();
	engine.statusChangeTime = EmuTime::zero;
}

template <typename Mode>
void LmcmCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
{
	if (!engine.transfer) return;
	if (unlikely(engine.time >= limit)) return;

	VDPVRAM& vram = engine.vram;
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.SX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.SY, engine.NY, engine.ARG);
	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	engine.ANX = clipNX_1_pixel<Mode>(engine.ASX, engine.ANX, engine.ARG);
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;

	// TODO we should (most likely) perform the actual read earlier and
	//  buffer it, and on a CPU-IO-read start the next read (just like how
	//  regular reading from VRAM works).
	engine.COL = likely(doPoint)
		   ? Mode::point(vram, engine.ASX, engine.SY, srcExt)
		   : 0xFF;
	engine.transfer = false;
	engine.ASX += TX; --engine.ANX;
	if (engine.ANX == 0) {
		engine.SY += TY; --(engine.NY);
		engine.ASX = engine.SX; engine.ANX = NX;
		if (--NY == 0) {
			engine.commandDone(engine.time);
		}
	}
	engine.time = limit; engine.nextAccessSlot(); // TODO
}

/** Logical move CPU -> VRAM.
  */
template <typename Mode> struct LmmcBaseCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
};
template <typename Mode, typename LogOp> struct LmmcCmd : public LmmcBaseCmd<Mode>
{
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void LmmcBaseCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_pixel<Mode>(engine.DX, engine.NX, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.statusChangeTime = EmuTime::zero;
	engine.transfer = true;
	engine.status |= 0x80;
	engine.time = time; engine.nextAccessSlot();
}

template <typename Mode, typename LogOp>
void LmmcCmd<Mode, LogOp>::execute(EmuTime::param limit, VDPCmdEngine& engine)
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
		// TODO: timing is inaccurate, this executes the read and write
		//  in the same access slot. Instead we should
		//    - wait for a byte
		//    - in next access slot read
		//    - in next access slot write
		if (likely(doPset)) {
			unsigned addr = Mode::addressOf(engine.ADX, engine.DY, dstExt);
			engine.tmpDst = vram.cmdWriteWindow.readNP(addr);
			Mode::pset(limit, vram, engine.ADX, addr,
			           engine.tmpDst, col, LogOp());
		}
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		engine.transfer = false;

		engine.ADX += TX; --engine.ANX;
		if (engine.ANX == 0) {
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(limit);
			}
		}
	}
	engine.time = limit; engine.nextAccessSlot(); // inaccurate, but avoid assert
}

/** High-speed move VDP -> VRAM.
  */
template <typename Mode> struct HmmvCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmvCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	unsigned NY = clipNY_1(engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.time = time; engine.nextAccessSlot();
	calcFinishTime(engine, NX, NY, 48);
}

template <typename Mode>
void HmmvCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
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
	auto calculator = engine.getSlotCalculator(limit);

	while (!calculator.limitReached()) {
		if (likely(doPset)) {
			vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
			              engine.COL, calculator.getTime());
		}
		engine.ADX += TX;
		Delta delta = DELTA_48;
		if (--engine.ANX == 0) {
			delta = DELTA_104; // 48 + 56;
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(delta);
	}
	engine.time = calculator.getTime();
	calcFinishTime(engine, NX, NY, 48);

	/*if (unlikely(dstExt)) {
		bool doPset = !dstExt || engine.hasExtendedVRAM;
		while (engine.time < limit) {
			if (likely(doPset)) {
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      engine.COL, engine.time);
			}
			engine.time += delta;
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.time < limit) {
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			EmuDuration dur = limit - engine.time;
			unsigned num = (delta != EmuDuration::zero)
			             ? std::min(dur.divUp(delta), engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				vram.cmdWrite(dstAddr.getAddr(), engine.COL,
					      engine.time);
				engine.time += delta;
				dstAddr.step(TX);
			}
			engine.ANX -= num;
			if (engine.ANX == 0) {
				engine.DY += TY;
				engine.NY -= 1;
				engine.ADX = engine.DX;
				engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(engine.time >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move VRAM -> VRAM.
  */
template <typename Mode> struct HmmmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_2_byte<Mode>(
		engine.SX, engine.DX, engine.NX, engine.ARG );
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ASX = engine.SX;
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.time = time; engine.nextAccessSlot();
	calcFinishTime(engine, NX, NY, 24 + 64);
	engine.phase = 0;
}

template <typename Mode>
void HmmmCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
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
	bool srcExt  = (engine.ARG & MXS) != 0;
	bool dstExt  = (engine.ARG & MXD) != 0;
	bool doPoint = !srcExt || engine.hasExtendedVRAM;
	bool doPset  = !dstExt || engine.hasExtendedVRAM;
	auto calculator = engine.getSlotCalculator(limit);

	switch (engine.phase) {
	case 0:
loop:		if (unlikely(calculator.limitReached())) { engine.phase = 0; break; }
		engine.tmpSrc = likely(doPoint)
			? vram.cmdReadWindow.readNP(
			       Mode::addressOf(engine.ASX, engine.SY, srcExt))
			: 0xFF;
		calculator.next(DELTA_24);
		// fall-through
	case 1: {
		if (unlikely(calculator.limitReached())) { engine.phase = 1; break; }
		if (likely(doPset)) {
			vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
			              engine.tmpSrc, calculator.getTime());
		}
		engine.ASX += TX; engine.ADX += TX;
		Delta delta = DELTA_64;
		if (--engine.ANX == 0) {
			delta = DELTA_128; // 64 + 64
			engine.SY += TY; engine.DY += TY; --(engine.NY);
			engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engine.time = calculator.getTime();
	calcFinishTime(engine, NX, NY, 24 + 64);

	/*if (unlikely(srcExt || dstExt)) {
		bool doPoint = !srcExt || engine.hasExtendedVRAM;
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.time < limit) {
			if (likely(doPset)) {
				byte p = likely(doPoint)
				       ? vram.cmdReadWindow.readNP(
					       Mode::addressOf(engine.ASX, engine.SY, srcExt))
				       : 0xFF;
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      p, engine.time);
			}
			engine.time += delta;
			engine.ASX += TX; engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ASX = engine.SX; engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.time < limit) {
			typename Mode::IncrByteAddr srcAddr(engine.ASX, engine.SY, TX);
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			EmuDuration dur = limit - engine.time;
			unsigned num = (delta != EmuDuration::zero)
			             ? std::min(dur.divUp(delta), engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engine.time);
				engine.time += delta;
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
					engine.commandDone(engine.time);
					break;
				}
			} else {
				engine.ASX += num * TX;
				engine.ADX += num * TX;
				assert(engine.time >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move VRAM -> VRAM (Y direction only).
  */
template <typename Mode> struct YmmmCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void YmmmCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.setMask(0x3FFFF, -1u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, 512, engine.ARG);
		// large enough so that it gets clipped
	unsigned NY = clipNY_2(engine.SY, engine.DY, engine.NY, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.time = time; engine.nextAccessSlot();
	calcFinishTime(engine, NX, NY, 24 + 40);
	engine.phase = 0;
}

template <typename Mode>
void YmmmCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
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

	// TODO does this use MXD for both read and write?
	//  it says so in the datasheet, but it seems unlogical
	//  OTOH YMMM also uses DX for both read and write
	bool dstExt = (engine.ARG & MXD) != 0;
	bool doPset  = !dstExt || engine.hasExtendedVRAM;
	auto calculator = engine.getSlotCalculator(limit);

	switch (engine.phase) {
	case 0:
loop:		if (unlikely(calculator.limitReached())) { engine.phase = 0; break; }
		if (likely(doPset)) {
			engine.tmpSrc = vram.cmdReadWindow.readNP(
			       Mode::addressOf(engine.ADX, engine.SY, dstExt));
		}
		calculator.next(DELTA_24);
		// fall-through
	case 1:
		if (unlikely(calculator.limitReached())) { engine.phase = 1; break; }
		if (likely(doPset)) {
			vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
			              engine.tmpSrc, calculator.getTime());
		}
		engine.ADX += TX;
		if (--engine.ANX == 0) {
			// note: going to the next line does not take extra time
			engine.SY += TY; engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(DELTA_40);
		goto loop;
	default:
		UNREACHABLE;
	}
	engine.time = calculator.getTime();
	calcFinishTime(engine, NX, NY, 24 + 40);

	/*
	if (unlikely(dstExt)) {
		bool doPset  = !dstExt || engine.hasExtendedVRAM;
		while (engine.time < limit) {
			if (likely(doPset)) {
				byte p = vram.cmdReadWindow.readNP(
					      Mode::addressOf(engine.ADX, engine.SY, dstExt));
				vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
					      p, engine.time);
			}
			engine.time += delta;
			engine.ADX += TX;
			if (--engine.ANX == 0) {
				engine.SY += TY; engine.DY += TY; --(engine.NY);
				engine.ADX = engine.DX; engine.ANX = NX;
				if (--NY == 0) {
					engine.commandDone(engine.time);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engine.time < limit) {
			typename Mode::IncrByteAddr srcAddr(engine.ADX, engine.SY, TX);
			typename Mode::IncrByteAddr dstAddr(engine.ADX, engine.DY, TX);
			EmuDuration dur = limit - engine.time;
			unsigned num = (delta != EmuDuration::zero)
			             ? std::min(dur.divUp(delta), engine.ANX)
			             : engine.ANX;
			for (unsigned i = 0; i < num; ++i) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engine.time);
				engine.time += delta;
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
					engine.commandDone(engine.time);
					break;
				}
			} else {
				engine.ADX += num * TX;
				assert(engine.time >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move CPU -> VRAM.
  */
template <typename Mode> struct HmmcCmd : public BlockCmd
{
	virtual void start(EmuTime::param time, VDPCmdEngine& engine);
	virtual void execute(EmuTime::param limit, VDPCmdEngine& engine);
};

template <typename Mode>
void HmmcCmd<Mode>::start(EmuTime::param time, VDPCmdEngine& engine)
{
	VDPVRAM& vram = engine.vram;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, -1u << 18, time);
	engine.NY &= 1023;
	unsigned NX = clipNX_1_byte<Mode>(engine.DX, engine.NX, engine.ARG);
	engine.ADX = engine.DX;
	engine.ANX = NX;
	engine.statusChangeTime = EmuTime::zero;
	engine.transfer = true;
	engine.status |= 0x80;
	engine.time = time; engine.nextAccessSlot();
}

template <typename Mode>
void HmmcCmd<Mode>::execute(EmuTime::param limit, VDPCmdEngine& engine)
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
		// TODO: timing is inaccurate. We should
		//  - wait for a byte
		//  - on the next access slot write that byte
		if (likely(doPset)) {
			vram.cmdWrite(Mode::addressOf(engine.ADX, engine.DY, dstExt),
			              engine.COL, limit);
		}
		engine.transfer = false;

		engine.ADX += TX; --engine.ANX;
		if (engine.ANX == 0) {
			engine.DY += TY; --(engine.NY);
			engine.ADX = engine.DX; engine.ANX = NX;
			if (--NY == 0) {
				engine.commandDone(limit);
			}
		}
	}
	engine.time = limit; engine.nextAccessSlot(); // inaccurate, but avoid assert
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
	, cmdTraceSetting(make_unique<BooleanSetting>(
		commandController, vdp_.getName() == "VDP" ? "vdpcmdtrace" :
		vdp_.getName() + " vdpcmdtrace", "VDP command tracing on/off",
		false))
	, cmdInProgressCallback(make_unique<TclCallback>(
		commandController, vdp_.getName() == "VDP" ?
		"vdpcmdinprogress_callback" : vdp_.getName() +
		" vdpcmdinprogress_callback",
	        "Tcl proc to call when a write to the VDP command engine is "
		"detected while the previous command is still in progress."))
	, time(EmuTime::zero)
	, statusChangeTime(EmuTime::infinity)
	, hasExtendedVRAM(vram.getSize() == (192 * 1024))
{
	status = 0;
	transfer = false;
	SX = SY = DX = DY = NX = NY = 0;
	ASX = ADX = ANX = 0;
	COL = ARG = CMD = 0;

	auto* abort = new AbortCmd();
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
	currentCommand = nullptr;
}

VDPCmdEngine::~VDPCmdEngine()
{
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

void VDPCmdEngine::setCmdReg(byte index, byte value, EmuTime::param time)
{
	sync(time);
	if (currentCommand && (index != 12)) {
		cmdInProgressCallback->execute(index, value);
	}
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
		sync(time);
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

	if (cmdTraceSetting->getBoolean()) {
		reportVdpCommand();
	}

	// Start command.
	status |= 0x01;
	currentCommand = commands[CMD][scrMode];
	currentCommand->start(time, *this);
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
		<< " [" << int((ARG & DIX) ? -int(NX) : int(NX))
		<<  ',' << int((ARG & DIY) ? -int(NY) : int(NY)) << ']' << std::endl;
}

void VDPCmdEngine::commandDone(EmuTime::param time)
{
	// Note: TR is not reset yet; it is reset when S#2 is read next.
	status &= 0xFE; // reset CE
	CMD = 0;
	currentCommand = nullptr;
	statusChangeTime = EmuTime::infinity;
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.disable(time);
}


// version 1: initial version
// version 2: replaced member 'Clock<> clock' with 'Emutime time'
// version 3: added 'phase', 'tmpSrc', 'tmpDst'
template<typename Archive>
void VDPCmdEngine::serialize(Archive& ar, unsigned version)
{
	// In older (development) versions CMD was split in a CMD and a LOG
	// member, though it was combined for the savestate. Only the CMD part
	// was guaranteed to be zero when no command was executing. So when
	// loading an older savestate this can still be the case.
	if (!currentCommand) {
		assert((CMD & 0xF0) == 0); // assert(CMD == 0);
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("time", time);
	} else {
		// in version 1, the 'time' member had type 'Clock<>'
		assert(ar.isLoader());
		VDP::VDPClock clock(EmuTime::dummy());
		ar.serialize("clock", clock);
		time = clock.getTime();
	}
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

	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("phase", phase);
		ar.serialize("tmpSrc", tmpSrc);
		ar.serialize("tmpDst", tmpDst);
	} else {
		assert(ar.isLoader());
		phase = tmpSrc = tmpDst = 0;
	}

	if (ar.isLoader()) {
		if (CMD & 0xF0) {
			assert(scrMode >= 0);
			currentCommand = commands[CMD][scrMode];
		} else {
			currentCommand = nullptr;
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(VDPCmdEngine);

} // namespace openmsx
