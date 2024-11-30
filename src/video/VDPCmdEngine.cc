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
  method. It should of course be updated after every read and write.
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
    (pixel or byte) is processed per horizontal line. The real x-coordinate
    is "SX/DX & 255".
*/

#include "VDPCmdEngine.hh"

#include "VDPVRAM.hh"

#include "EmuTime.hh"
#include "serialize.hh"

#include "unreachable.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <string_view>

namespace openmsx {

using namespace VDPAccessSlots;

template<typename Mode>
static constexpr unsigned clipNX_1_pixel(unsigned DX, unsigned NX, byte ARG)
{
	if (DX >= Mode::PIXELS_PER_LINE) [[unlikely]] {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & VDPCmdEngine::DIX)
		? std::min(NX, DX + 1)
		: std::min(NX, Mode::PIXELS_PER_LINE - DX);
}

template<typename Mode>
static constexpr unsigned clipNX_1_byte(unsigned DX, unsigned NX, byte ARG)
{
	constexpr unsigned BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if (BYTES_PER_LINE <= DX) [[unlikely]] {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & VDPCmdEngine::DIX)
		? std::min(NX, DX + 1)
		: std::min(NX, BYTES_PER_LINE - DX);
}

template<typename Mode>
static constexpr unsigned clipNX_2_pixel(unsigned SX, unsigned DX, unsigned NX, byte ARG)
{
	if ((SX >= Mode::PIXELS_PER_LINE) ||
	    (DX >= Mode::PIXELS_PER_LINE)) [[unlikely]] {
		return 1;
	}
	NX = NX ? NX : Mode::PIXELS_PER_LINE;
	return (ARG & VDPCmdEngine::DIX)
		? std::min(NX, std::min(SX, DX) + 1)
		: std::min(NX, Mode::PIXELS_PER_LINE - std::max(SX, DX));
}

template<typename Mode>
static constexpr unsigned clipNX_2_byte(unsigned SX, unsigned DX, unsigned NX, byte ARG)
{
	constexpr unsigned BYTES_PER_LINE =
		Mode::PIXELS_PER_LINE >> Mode::PIXELS_PER_BYTE_SHIFT;

	SX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	DX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	if ((BYTES_PER_LINE <= SX) ||
	    (BYTES_PER_LINE <= DX)) [[unlikely]] {
		return 1;
	}
	NX >>= Mode::PIXELS_PER_BYTE_SHIFT;
	NX = NX ? NX : BYTES_PER_LINE;
	return (ARG & VDPCmdEngine::DIX)
		? std::min(NX, std::min(SX, DX) + 1)
		: std::min(NX, BYTES_PER_LINE - std::max(SX, DX));
}

static constexpr unsigned clipNY_1(unsigned DY, unsigned NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & VDPCmdEngine::DIY) ? std::min(NY, DY + 1) : NY;
}

static constexpr unsigned clipNY_2(unsigned SY, unsigned DY, unsigned NY, byte ARG)
{
	NY = NY ? NY : 1024;
	return (ARG & VDPCmdEngine::DIY) ? std::min(NY, std::min(SY, DY) + 1) : NY;
}


//struct IncrByteAddr4;
//struct IncrByteAddr5;
//struct IncrByteAddr6;
//struct IncrByteAddr7;
//struct IncrPixelAddr4;
//struct IncrPixelAddr5;
//struct IncrPixelAddr6;
//struct IncrMask4;
//struct IncrMask5;
//struct IncrMask7;
//struct IncrShift4;
//struct IncrShift5;
//struct IncrShift7;
//using IncrPixelAddr7 = IncrByteAddr7;
//using IncrMask6  = IncrMask4;
//using IncrShift6 = IncrShift4;


template<typename LogOp> static void psetFast(
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
	//using IncrByteAddr  = IncrByteAddr4;
	//using IncrPixelAddr = IncrPixelAddr4;
	//using IncrMask      = IncrMask4;
	//using IncrShift     = IncrShift4;
	static constexpr byte COLOR_MASK = 0x0F;
	static constexpr byte PIXELS_PER_BYTE = 2;
	static constexpr byte PIXELS_PER_BYTE_SHIFT = 1;
	static constexpr unsigned PIXELS_PER_LINE = 256;
	static unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static byte point(const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template<typename LogOp>
	static void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
	static byte duplicate(byte color);
};

inline unsigned Graphic4Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	if (!extVRAM) [[likely]] {
		return ((y & 1023) << 7) | ((x & 255) >> 1);
	} else {
		return ((y &  511) << 7) | ((x & 255) >> 1) | 0x20000;
	}
}

inline byte Graphic4Mode::point(
	const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return (vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 1) << 2)) & 15;
}

template<typename LogOp>
inline void Graphic4Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	auto sh = byte(((~x) & 1) << 2);
	op(time, vram, addr, src, byte(color << sh), ~byte(15 << sh));
}

inline byte Graphic4Mode::duplicate(byte color)
{
	assert((color & 0xF0) == 0);
	return byte(color | (color << 4));
}

/** Represents V9938 Graphic 5 mode (SCREEN6).
  */
struct Graphic5Mode
{
	//using IncrByteAddr  = IncrByteAddr5;
	//using IncrPixelAddr = IncrPixelAddr5;
	//using IncrMask      = IncrMask5;
	//using IncrShift     = IncrShift5;
	static constexpr byte COLOR_MASK = 0x03;
	static constexpr byte PIXELS_PER_BYTE = 4;
	static constexpr byte PIXELS_PER_BYTE_SHIFT = 2;
	static constexpr unsigned PIXELS_PER_LINE = 512;
	static unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static byte point(const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template<typename LogOp>
	static void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
	static byte duplicate(byte color);
};

inline unsigned Graphic5Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	if (!extVRAM) [[likely]] {
		return ((y & 1023) << 7) | ((x & 511) >> 2);
	} else {
		return ((y &  511) << 7) | ((x & 511) >> 2) | 0x20000;
	}
}

inline byte Graphic5Mode::point(
	const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return (vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 3) << 1)) & 3;
}

template<typename LogOp>
inline void Graphic5Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	auto sh = byte(((~x) & 3) << 1);
	op(time, vram, addr, src, byte(color << sh), ~byte(3 << sh));
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
	//using IncrByteAddr  = IncrByteAddr6;
	//using IncrPixelAddr = IncrPixelAddr6;
	//using IncrMask      = IncrMask6;
	//using IncrShift     = IncrShift6;
	static constexpr byte COLOR_MASK = 0x0F;
	static constexpr byte PIXELS_PER_BYTE = 2;
	static constexpr byte PIXELS_PER_BYTE_SHIFT = 1;
	static constexpr unsigned PIXELS_PER_LINE = 512;
	static unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static byte point(const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template<typename LogOp>
	static void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
	static byte duplicate(byte color);
};

inline unsigned Graphic6Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	if (!extVRAM) [[likely]] {
		return ((x & 2) << 15) | ((y & 511) << 7) | ((x & 511) >> 2);
	} else {
		return 0x20000         | ((y & 511) << 7) | ((x & 511) >> 2);
	}
}

inline byte Graphic6Mode::point(
	const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return (vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM))
		>> (((~x) & 1) << 2)) & 15;
}

template<typename LogOp>
inline void Graphic6Mode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned x, unsigned addr,
	byte src, byte color, LogOp op)
{
	auto sh = byte(((~x) & 1) << 2);
	op(time, vram, addr, src, byte(color << sh), ~byte(15 << sh));
}

inline byte Graphic6Mode::duplicate(byte color)
{
	assert((color & 0xF0) == 0);
	return byte(color | (color << 4));
}

/** Represents V9938 Graphic 7 mode (SCREEN8).
  */
struct Graphic7Mode
{
	//using IncrByteAddr  = IncrByteAddr7;
	//using IncrPixelAddr = IncrPixelAddr7;
	//using IncrMask      = IncrMask7;
	//using IncrShift     = IncrShift7;
	static constexpr byte COLOR_MASK = 0xFF;
	static constexpr byte PIXELS_PER_BYTE = 1;
	static constexpr byte PIXELS_PER_BYTE_SHIFT = 0;
	static constexpr unsigned PIXELS_PER_LINE = 256;
	static unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static byte point(const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template<typename LogOp>
	static void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
	static byte duplicate(byte color);
};

inline unsigned Graphic7Mode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	if (!extVRAM) [[likely]] {
		return ((x & 1) << 16) | ((y & 511) << 7) | ((x & 255) >> 1);
	} else {
		return 0x20000         | ((y & 511) << 7) | ((x & 255) >> 1);
	}
}

inline byte Graphic7Mode::point(
	const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM));
}

template<typename LogOp>
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

/** Represents V9958 non-bitmap command mode. This uses the Graphic7Mode
  * coordinate system, but in non-planar mode.
  */
struct NonBitmapMode
{
	//using IncrByteAddr  = IncrByteAddrNonBitMap;
	//using IncrPixelAddr = IncrPixelAddrNonBitMap;
	//using IncrMask      = IncrMaskNonBitMap;
	//using IncrShift     = IncrShiftNonBitMap;
	static constexpr byte COLOR_MASK = 0xFF;
	static constexpr byte PIXELS_PER_BYTE = 1;
	static constexpr byte PIXELS_PER_BYTE_SHIFT = 0;
	static constexpr unsigned PIXELS_PER_LINE = 256;
	static unsigned addressOf(unsigned x, unsigned y, bool extVRAM);
	static byte point(const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM);
	template<typename LogOp>
	static void pset(EmuTime::param time, VDPVRAM& vram,
		unsigned x, unsigned addr, byte src, byte color, LogOp op);
	static byte duplicate(byte color);
};

inline unsigned NonBitmapMode::addressOf(
	unsigned x, unsigned y, bool extVRAM)
{
	if (!extVRAM) [[likely]] {
		return ((y & 511) << 8) | (x & 255);
	} else {
		return ((y & 255) << 8) | (x & 255) | 0x20000;
	}
}

inline byte NonBitmapMode::point(
	const VDPVRAM& vram, unsigned x, unsigned y, bool extVRAM)
{
	return vram.cmdReadWindow.readNP(addressOf(x, y, extVRAM));
}

template<typename LogOp>
inline void NonBitmapMode::pset(
	EmuTime::param time, VDPVRAM& vram, unsigned /*x*/, unsigned addr,
	byte src, byte color, LogOp op)
{
	op(time, vram, addr, src, color, 0);
}

inline byte NonBitmapMode::duplicate(byte color)
{
	return color;
}

/** Incremental address calculation (byte based, no extended VRAM)
 */
struct IncrByteAddr4
{
	IncrByteAddr4(unsigned x, unsigned y, int /*tx*/)
		: addr(Graphic4Mode::addressOf(x, y, false))
	{
	}
	[[nodiscard]] unsigned getAddr() const
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
		: addr(Graphic5Mode::addressOf(x, y, false))
	{
	}
	[[nodiscard]] unsigned getAddr() const
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
		: addr(Graphic7Mode::addressOf(x, y, false))
		, delta((tx > 0) ? 0x10000 : (0x10000 - 1))
		, delta2((tx > 0) ? ( 0x10000 ^ (1 - 0x10000))
		                  : (-0x10000 ^ (0x10000 - 1)))
	{
		if (x & 1) delta ^= delta2;
	}
	[[nodiscard]] unsigned getAddr() const
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
		: addr(Graphic4Mode::addressOf(x, y, false))
		, delta((tx == 1) ? (x & 1) : ((x & 1) - 1))
	{
	}
	[[nodiscard]] unsigned getAddr() const { return addr; }
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
		: addr(Graphic5Mode::addressOf(x, y, false))
		                       // x |  0 |  1 |  2 |  3
		                       //-----------------------
		, c1(-(signed(x) & 1)) //   |  0 | -1 |  0 | -1
		, c2((x & 2) >> 1)     //   |  0 |  0 |  1 |  1
	{
		if (tx < 0) {
			c1 = ~c1;      //   | -1 |  0 | -1 |  0
			c2 -= 1;       //   | -1 | -1 |  0 |  0
		}
	}
	[[nodiscard]] unsigned getAddr() const { return addr; }
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
		: addr(Graphic6Mode::addressOf(x, y, false))
		, c1(-(signed(x) & 1))
		, c3((tx == 1) ? unsigned(0x10000 ^ (1 - 0x10000))   // == -0x1FFFF
		               : unsigned(-0x10000 ^ (0x10000 - 1))) // == -1
	{
		if (tx == 1) {
			c2 = (x & 2) ? (1 - 0x10000) :  0x10000;
		} else {
			c1 = ~c1;
			c2 = (x & 2) ? -0x10000 : (0x10000 - 1);
		}
	}
	[[nodiscard]] unsigned getAddr() const { return addr; }
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
		: mask(byte(0x0F << ((x & 1) << 2)))
	{
	}
	[[nodiscard]] byte getMask() const
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
		: mask(~byte(0xC0 >> ((x & 3) << 1)))
		, shift((tx > 0) ? 6 : 2)
	{
	}
	[[nodiscard]] byte getMask() const
	{
		return mask;
	}
	void step()
	{
		mask = byte((mask << shift) | (mask >> (8 - shift)));
	}
private:
	byte mask;
	const byte shift;
};

struct IncrMask7
{
	IncrMask7(unsigned /*x*/, int /*tx*/) {}
	[[nodiscard]] byte getMask() const
	{
		return 0;
	}
	void step() const {}
};


/* Shift between source and destination pixel for LMMM command.
 */
struct IncrShift4
{
	IncrShift4(unsigned sx, unsigned dx)
		: shift(((dx - sx) & 1) * 4)
	{
	}
	[[nodiscard]] byte doShift(byte color) const
	{
		return byte((color >> shift) | (color << shift));
	}
private:
	const byte shift;
};

struct IncrShift5
{
	IncrShift5(unsigned sx, unsigned dx)
		: shift(((dx - sx) & 3) * 2)
	{
	}
	[[nodiscard]] byte doShift(byte color) const
	{
		return byte((color >> shift) | (color << (8 - shift)));
	}
private:
	const byte shift;
};

struct IncrShift7
{
	IncrShift7(unsigned /*sx*/, unsigned /*dx*/) {}
	[[nodiscard]] byte doShift(byte color) const
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

template<typename Op>
struct TransparentOp : Op {
	void operator()(EmuTime::param time, VDPVRAM& vram, unsigned addr,
	                byte src, byte color, byte mask) const
	{
		// TODO does this skip the write or re-write the original value
		//      might make a difference in case the CPU has written
		//      the same address between the command read and write
		if (color) Op::operator()(time, vram, addr, src, color, mask);
	}
};
using TImpOp = TransparentOp<ImpOp>;
using TAndOp = TransparentOp<AndOp>;
using TOrOp  = TransparentOp<OrOp>;
using TXorOp = TransparentOp<XorOp>;
using TNotOp = TransparentOp<NotOp>;


// Commands

void VDPCmdEngine::setStatusChangeTime(EmuTime::param t)
{
	statusChangeTime = t;
	if ((t != EmuTime::infinity()) && executingProbe.anyObservers()) {
		vdp.scheduleCmdSync(t);
	}
}

void VDPCmdEngine::calcFinishTime(unsigned nx, unsigned ny, unsigned ticksPerPixel)
{
	if (!CMD) return;
	if (vdp.getBrokenCmdTiming()) {
		setStatusChangeTime(EmuTime::zero()); // will finish soon
		return;
	}

	// Underestimation for when the command will be finished. This assumes
	// we never have to wait for access slots and that there's no overhead
	// per line.
	auto t = VDP::VDPClock::duration(ticksPerPixel);
	t *= ((nx * (ny - 1)) + (ANX - 1));
	setStatusChangeTime(engineTime + t);
}

/** Abort
  */
void VDPCmdEngine::startAbrt(EmuTime::param time)
{
	commandDone(time);
}

/** Point
  */
void VDPCmdEngine::startPoint(EmuTime::param time)
{
	vram.cmdReadWindow.setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.disable(time);
	nextAccessSlot(time);
	setStatusChangeTime(EmuTime::zero()); // will finish soon
}

template<typename Mode>
void VDPCmdEngine::executePoint(EmuTime::param limit)
{
	if (engineTime >= limit) [[unlikely]] return;

	bool srcExt  = (ARG & MXS) != 0;
	if (bool doPoint = !srcExt || hasExtendedVRAM; doPoint) [[likely]] {
		COL = Mode::point(vram, SX, SY, srcExt);
	} else {
		COL = 0xFF;
	}
	commandDone(engineTime);
}

/** Pset
  */
void VDPCmdEngine::startPset(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	nextAccessSlot(time);
	setStatusChangeTime(EmuTime::zero()); // will finish soon
	phase = 0;
}

template<typename Mode, typename LogOp>
void VDPCmdEngine::executePset(EmuTime::param limit)
{
	bool dstExt = (ARG & MXD) != 0;
	bool doPset = !dstExt || hasExtendedVRAM;
	unsigned addr = Mode::addressOf(DX, DY, dstExt);

	switch (phase) {
	case 0:
		if (engineTime >= limit) [[unlikely]] { phase = 0; break; }
		if (doPset) [[likely]] {
			tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		nextAccessSlot(Delta::D24); // TODO
		[[fallthrough]];
	case 1:
		if (engineTime >= limit) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			byte col = COL & Mode::COLOR_MASK;
			Mode::pset(engineTime, vram, DX, addr, tmpDst, col, LogOp());
		}
		commandDone(engineTime);
		break;
	default:
		UNREACHABLE;
	}
}

/** Search a dot.
  */
void VDPCmdEngine::startSrch(EmuTime::param time)
{
	vram.cmdReadWindow.setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.disable(time);
	ASX = SX;
	nextAccessSlot(time);
	setStatusChangeTime(EmuTime::zero()); // we can find it any moment
}

template<typename Mode>
void VDPCmdEngine::executeSrch(EmuTime::param limit)
{
	byte CL = COL & Mode::COLOR_MASK;
	int TX = (ARG & DIX) ? -1 : 1;
	bool AEQ = (ARG & EQ) != 0; // TODO: Do we look for "==" or "!="?

	// TODO use MXS or MXD here?
	//  datasheet says MXD but MXS seems more logical
	bool srcExt  = (ARG & MXS) != 0;
	bool doPoint = !srcExt || hasExtendedVRAM;
	auto calculator = getSlotCalculator(limit);

	while (!calculator.limitReached()) {
		auto p = [&] () -> byte {
			if (doPoint) [[likely]] {
				return Mode::point(vram, ASX, SY, srcExt);
			} else {
				return 0xFF;
			}
		}();
		if ((p == CL) ^ AEQ) {
			status |= 0x10; // border detected
			commandDone(calculator.getTime());
			break;
		}
		ASX += TX;
		if (ASX & Mode::PIXELS_PER_LINE) {
			status &= 0xEF; // border not detected
			commandDone(calculator.getTime());
			break;
		}
		calculator.next(Delta::D88); // TODO
	}
	engineTime = calculator.getTime();
}

/** Draw a line.
  */
void VDPCmdEngine::startLine(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	ASX = ((NX - 1) >> 1);
	ADX = DX;
	ANX = 0;
	nextAccessSlot(time);
	setStatusChangeTime(EmuTime::zero()); // TODO can still be optimized
	phase = 0;
}

template<typename Mode, typename LogOp>
void VDPCmdEngine::executeLine(EmuTime::param limit)
{
	// See doc/line-speed.txt for some background info on the timing.
	byte CL = COL & Mode::COLOR_MASK;
	int TX = (ARG & DIX) ? -1 : 1;
	int TY = (ARG & DIY) ? -1 : 1;
	bool dstExt = (ARG & MXD) != 0;
	bool doPset = !dstExt || hasExtendedVRAM;
	unsigned addr = Mode::addressOf(ADX, DY, dstExt);
	auto calculator = getSlotCalculator(limit);

	switch (phase) {
	case 0:
loop:		if (calculator.limitReached()) [[unlikely]] { phase = 0; break; }
		if (doPset) [[likely]] {
			tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		calculator.next(Delta::D24);
		[[fallthrough]];
	case 1: {
		if (calculator.limitReached()) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			Mode::pset(calculator.getTime(), vram, ADX, addr,
			           tmpDst, CL, LogOp());
		}

		Delta delta = Delta::D88;
		if ((ARG & MAJ) == 0) {
			// X-Axis is major direction.
			ADX += TX;
			// confirmed on real HW:
			//  - end-test happens before DY += TY
			//  - (ADX & PPL) test only happens after first pixel
			//    is drawn. And it does test with 'AND' (not with ==)
			if (ANX++ == NX || (ADX & Mode::PIXELS_PER_LINE)) {
				commandDone(calculator.getTime());
				break;
			}
			if (ASX < NY) {
				ASX += NX;
				DY += TY;
				delta = Delta::D120; // 88 + 32
			}
			ASX -= NY;
			ASX &= 1023; // mask to 10 bits range
		} else {
			// Y-Axis is major direction.
			// confirmed on real HW: DY += TY happens before end-test
			DY += TY;
			if (ASX < NY) {
				ASX += NX;
				ADX += TX;
				delta = Delta::D120; // 88 + 32
			}
			ASX -= NY;
			ASX &= 1023; // mask to 10 bits range
			if (ANX++ == NX || (ADX & Mode::PIXELS_PER_LINE)) {
				commandDone(calculator.getTime());
				break;
			}
		}
		addr = Mode::addressOf(ADX, DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engineTime = calculator.getTime();
}


/** Logical move VDP -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startLmmv(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	ADX = DX;
	ANX = tmpNX;
	nextAccessSlot(time);
	calcFinishTime(tmpNX, tmpNY, 72 + 24);
	phase = 0;
}

template<typename Mode, typename LogOp>
void VDPCmdEngine::executeLmmv(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	int TX = (ARG & DIX) ? -1 : 1;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ADX, ANX, ARG);
	byte CL = COL & Mode::COLOR_MASK;
	bool dstExt = (ARG & MXD) != 0;
	bool doPset = !dstExt || hasExtendedVRAM;
	unsigned addr = Mode::addressOf(ADX, DY, dstExt);
	auto calculator = getSlotCalculator(limit);

	switch (phase) {
	case 0:
loop:		if (calculator.limitReached()) [[unlikely]] { phase = 0; break; }
		if (doPset) [[likely]] {
			tmpDst = vram.cmdWriteWindow.readNP(addr);
		}
		calculator.next(Delta::D24);
		[[fallthrough]];
	case 1: {
		if (calculator.limitReached()) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			Mode::pset(calculator.getTime(), vram, ADX, addr,
			           tmpDst, CL, LogOp());
		}
		ADX += TX;
		Delta delta = Delta::D72;
		if (--ANX == 0) {
			delta = Delta::D136; // 72 + 64;
			DY += TY; --NY;
			ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(calculator.getTime());
				break;
			}
		}
		addr = Mode::addressOf(ADX, DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engineTime = calculator.getTime();
	this->calcFinishTime(tmpNX, tmpNY, 72 + 24);

	/*
	if (dstExt) [[unlikely]] {
		bool doPset = !dstExt || hasExtendedVRAM;
		while (engineTime < limit) {
			if (doPset) [[likely]] {
				Mode::pset(engineTime, vram, ADX, DY,
					   dstExt, CL, LogOp());
			}
			engineTime += delta;
			ADX += TX;
			if (--ANX == 0) {
				DY += TY; --NY;
				ADX = DX; ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		CL = Mode::duplicate(CL);
		while (engineTime < limit) {
			typename Mode::IncrPixelAddr dstAddr(ADX, DY, TX);
			typename Mode::IncrMask      dstMask(ADX, TX);
			EmuDuration dur = limit - engineTime;
			unsigned num = (delta != EmuDuration::zero())
			             ? std::min(dur.divUp(delta), ANX)
			             : ANX;
			for (auto i : xrange(num)) {
				byte mask = dstMask.getMask();
				psetFast(engineTime, vram, dstAddr.getAddr(),
					 CL & ~mask, mask, LogOp());
				engineTime += delta;
				dstAddr.step(TX);
				dstMask.step();
			}
			ANX -= num;
			if (ANX == 0) {
				DY += TY;
				NY -= 1;
				ADX = DX;
				ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			} else {
				ADX += num * TX;
				assert(engineTime >= limit);
				break;
			}
		}
	}
	*/
}

/** Logical move VRAM -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startLmmm(EmuTime::param time)
{
	vram.cmdReadWindow .setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_2_pixel<Mode>(SX, DX, NX, ARG);
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	ASX = SX;
	ADX = DX;
	ANX = tmpNX;
	nextAccessSlot(time);
	calcFinishTime(tmpNX, tmpNY, 64 + 32 + 24);
	phase = 0;
}

template<typename Mode, typename LogOp>
void VDPCmdEngine::executeLmmm(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_2_pixel<Mode>(SX, DX, NX, ARG);
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	int TX = (ARG & DIX) ? -1 : 1;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_2_pixel<Mode>(ASX, ADX, ANX, ARG);
	bool srcExt  = (ARG & MXS) != 0;
	bool dstExt  = (ARG & MXD) != 0;
	bool doPoint = !srcExt || hasExtendedVRAM;
	bool doPset  = !dstExt || hasExtendedVRAM;
	unsigned dstAddr = Mode::addressOf(ADX, DY, dstExt);
	auto calculator = getSlotCalculator(limit);

	switch (phase) {
	case 0:
loop:		if (calculator.limitReached()) [[unlikely]] { phase = 0; break; }
		if (doPoint) [[likely]] {
		       tmpSrc = Mode::point(vram, ASX, SY, srcExt);
		} else {
		       tmpSrc = 0xFF;
		}
		calculator.next(Delta::D32);
		[[fallthrough]];
	case 1:
		if (calculator.limitReached()) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			tmpDst = vram.cmdWriteWindow.readNP(dstAddr);
		}
		calculator.next(Delta::D24);
		[[fallthrough]];
	case 2: {
		if (calculator.limitReached()) [[unlikely]] { phase = 2; break; }
		if (doPset) [[likely]] {
			Mode::pset(calculator.getTime(), vram, ADX, dstAddr,
			           tmpDst, tmpSrc, LogOp());
		}
		ASX += TX; ADX += TX;
		Delta delta = Delta::D64;
		if (--ANX == 0) {
			delta = Delta::D128; // 64 + 64
			SY += TY; DY += TY; --NY;
			ASX = SX; ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(calculator.getTime());
				break;
			}
		}
		dstAddr = Mode::addressOf(ADX, DY, dstExt);
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engineTime = calculator.getTime();
	this->calcFinishTime(tmpNX, tmpNY, 64 + 32 + 24);

	/*if (srcExt || dstExt) [[unlikely]] {
		bool doPoint = !srcExt || hasExtendedVRAM;
		bool doPset  = !dstExt || hasExtendedVRAM;
		while (engineTime < limit) {
			if (doPset) [[likely]] {
				auto p = [&] () -> byte {
					if (doPoint) [[likely]] {
						return Mode::point(vram, ASX, SY, srcExt);
					} else {
						return 0xFF;
					}
				}();
				Mode::pset(engineTime, vram, ADX, DY,
					   dstExt, p, LogOp());
			}
			engineTime += delta;
			ASX += TX; ADX += TX;
			if (--ANX == 0) {
				SY += TY; DY += TY; --NY;
				ASX = SX; ADX = DX; ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engineTime < limit) {
			typename Mode::IncrPixelAddr srcAddr(ASX, SY, TX);
			typename Mode::IncrPixelAddr dstAddr(ADX, DY, TX);
			typename Mode::IncrMask      dstMask(ADX, TX);
			typename Mode::IncrShift     shift  (ASX, ADX);
			EmuDuration dur = limit - engineTime;
			unsigned num = (delta != EmuDuration::zero())
			             ? std::min(dur.divUp(delta), ANX)
			             : ANX;
			for (auto i : xrange(num)) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				p = shift.doShift(p);
				byte mask = dstMask.getMask();
				psetFast(engineTime, vram, dstAddr.getAddr(),
					 p & ~mask, mask, LogOp());
				engineTime += delta;
				srcAddr.step(TX);
				dstAddr.step(TX);
				dstMask.step();
			}
			ANX -= num;
			if (ANX == 0) {
				SY += TY;
				DY += TY;
				NY -= 1;
				ASX = SX;
				ADX = DX;
				ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			} else {
				ASX += num * TX;
				ADX += num * TX;
				assert(engineTime >= limit);
				break;
			}
		}
	}
	*/
}

/** Logical move VRAM -> CPU.
  */
template<typename Mode>
void VDPCmdEngine::startLmcm(EmuTime::param time)
{
	vram.cmdReadWindow.setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.disable(time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(SX, NX, ARG);
	ASX = SX;
	ANX = tmpNX;
	transfer = true;
	status |= 0x80;
	nextAccessSlot(time);
	setStatusChangeTime(EmuTime::zero());
}

template<typename Mode>
void VDPCmdEngine::executeLmcm(EmuTime::param limit)
{
	if (!transfer) return;
	if (engineTime >= limit) [[unlikely]] return;

	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(SX, NX, ARG);
	unsigned tmpNY = clipNY_1(SY, NY, ARG);
	int TX = (ARG & DIX) ? -1 : 1;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ASX, ANX, ARG);
	bool srcExt  = (ARG & MXS) != 0;

	// TODO we should (most likely) perform the actual read earlier and
	//  buffer it, and on a CPU-IO-read start the next read (just like how
	//  regular reading from VRAM works).
	if (bool doPoint = !srcExt || hasExtendedVRAM; doPoint) [[likely]] {
		COL = Mode::point(vram, ASX, SY, srcExt);
	} else {
		COL = 0xFF;
	}
	transfer = false;
	ASX += TX; --ANX;
	if (ANX == 0) {
		SY += TY; --NY;
		ASX = SX; ANX = tmpNX;
		if (--tmpNY == 0) {
			commandDone(engineTime);
		}
	}
	nextAccessSlot(limit); // TODO
}

/** Logical move CPU -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startLmmc(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(DX, NX, ARG);
	ADX = DX;
	ANX = tmpNX;
	setStatusChangeTime(EmuTime::zero());
	// do not set 'transfer = true', this fixes bug#1014
	// Baltak Rampage: characters in greetings part are one pixel offset
	status |= 0x80;
	nextAccessSlot(time);
}

template<typename Mode, typename LogOp>
void VDPCmdEngine::executeLmmc(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_1_pixel<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	int TX = (ARG & DIX) ? -1 : 1;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_pixel<Mode>(ADX, ANX, ARG);
	bool dstExt = (ARG & MXD) != 0;
	bool doPset  = !dstExt || hasExtendedVRAM;

	if (transfer) {
		byte col = COL & Mode::COLOR_MASK;
		// TODO: timing is inaccurate, this executes the read and write
		//  in the same access slot. Instead we should
		//    - wait for a byte
		//    - in next access slot read
		//    - in next access slot write
		if (doPset) [[likely]] {
			unsigned addr = Mode::addressOf(ADX, DY, dstExt);
			tmpDst = vram.cmdWriteWindow.readNP(addr);
			Mode::pset(limit, vram, ADX, addr,
			           tmpDst, col, LogOp());
		}
		// Execution is emulated as instantaneous, so don't bother
		// with the timing.
		// Note: Correct timing would require currentTime to be set
		//       to the moment transfer becomes true.
		transfer = false;

		ADX += TX; --ANX;
		if (ANX == 0) {
			DY += TY; --NY;
			ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(limit);
			}
		}
	}
	nextAccessSlot(limit); // inaccurate, but avoid assert
}

/** High-speed move VDP -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startHmmv(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	ADX = DX;
	ANX = tmpNX;
	nextAccessSlot(time);
	calcFinishTime(tmpNX, tmpNY, 48);
}

template<typename Mode>
void VDPCmdEngine::executeHmmv(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	int TX = (ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(
		ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, ARG);
	bool dstExt = (ARG & MXD) != 0;
	bool doPset = !dstExt || hasExtendedVRAM;
	auto calculator = getSlotCalculator(limit);

	while (!calculator.limitReached()) {
		if (doPset) [[likely]] {
			vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
			              COL, calculator.getTime());
		}
		ADX += TX;
		Delta delta = Delta::D48;
		if (--ANX == 0) {
			delta = Delta::D104; // 48 + 56;
			DY += TY; --NY;
			ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(delta);
	}
	engineTime = calculator.getTime();
	calcFinishTime(tmpNX, tmpNY, 48);

	/*if (dstExt) [[unlikely]] {
		bool doPset = !dstExt || hasExtendedVRAM;
		while (engineTime < limit) {
			if (doPset) [[likely]] {
				vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
					      COL, engineTime);
			}
			engineTime += delta;
			ADX += TX;
			if (--ANX == 0) {
				DY += TY; --NY;
				ADX = DX; ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engineTime < limit) {
			typename Mode::IncrByteAddr dstAddr(ADX, DY, TX);
			EmuDuration dur = limit - engineTime;
			unsigned num = (delta != EmuDuration::zero())
			             ? std::min(dur.divUp(delta), ANX)
			             : ANX;
			for (auto i : xrange(num)) {
				vram.cmdWrite(dstAddr.getAddr(), COL,
					      engineTime);
				engineTime += delta;
				dstAddr.step(TX);
			}
			ANX -= num;
			if (ANX == 0) {
				DY += TY;
				NY -= 1;
				ADX = DX;
				ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			} else {
				ADX += num * TX;
				assert(engineTime >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move VRAM -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startHmmm(EmuTime::param time)
{
	vram.cmdReadWindow .setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_2_byte<Mode>(SX, DX, NX, ARG);
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	ASX = SX;
	ADX = DX;
	ANX = tmpNX;
	nextAccessSlot(time);
	calcFinishTime(tmpNX, tmpNY, 24 + 64);
	phase = 0;
}

template<typename Mode>
void VDPCmdEngine::executeHmmm(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_2_byte<Mode>(SX, DX, NX, ARG);
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	int TX = (ARG & DIX)
	       ? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_2_byte<Mode>(
		ASX, ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, ARG);
	bool srcExt  = (ARG & MXS) != 0;
	bool dstExt  = (ARG & MXD) != 0;
	bool doPoint = !srcExt || hasExtendedVRAM;
	bool doPset  = !dstExt || hasExtendedVRAM;
	auto calculator = getSlotCalculator(limit);

	switch (phase) {
	case 0:
loop:		if (calculator.limitReached()) [[unlikely]] { phase = 0; break; }
		if (doPoint) [[likely]] {
			tmpSrc = vram.cmdReadWindow.readNP(Mode::addressOf(ASX, SY, srcExt));
		} else {
			tmpSrc = 0xFF;
		}
		calculator.next(Delta::D24);
		[[fallthrough]];
	case 1: {
		if (calculator.limitReached()) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
			              tmpSrc, calculator.getTime());
		}
		ASX += TX; ADX += TX;
		Delta delta = Delta::D64;
		if (--ANX == 0) {
			delta = Delta::D128; // 64 + 64
			SY += TY; DY += TY; --NY;
			ASX = SX; ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(delta);
		goto loop;
	}
	default:
		UNREACHABLE;
	}
	engineTime = calculator.getTime();
	calcFinishTime(tmpNX, tmpNY, 24 + 64);

	/*if (srcExt || dstExt) [[unlikely]] {
		bool doPoint = !srcExt || hasExtendedVRAM;
		bool doPset  = !dstExt || hasExtendedVRAM;
		while (engineTime < limit) {
			if (doPset) [[likely]] {
				auto p = [&] () -> byte {
					if (doPoint) [[likely]] {
						return vram.cmdReadWindow.readNP(Mode::addressOf(ASX, SY, srcExt));
					} else {
						return 0xFF;
					}
				}();
				vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
					      p, engineTime);
			}
			engineTime += delta;
			ASX += TX; ADX += TX;
			if (--ANX == 0) {
				SY += TY; DY += TY; --NY;
				ASX = SX; ADX = DX; ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engineTime < limit) {
			typename Mode::IncrByteAddr srcAddr(ASX, SY, TX);
			typename Mode::IncrByteAddr dstAddr(ADX, DY, TX);
			EmuDuration dur = limit - engineTime;
			unsigned num = (delta != EmuDuration::zero())
			             ? std::min(dur.divUp(delta), ANX)
			             : ANX;
			for (auto i : xrange(num)) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engineTime);
				engineTime += delta;
				srcAddr.step(TX);
				dstAddr.step(TX);
			}
			ANX -= num;
			if (ANX == 0) {
				SY += TY;
				DY += TY;
				NY -= 1;
				ASX = SX;
				ADX = DX;
				ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			} else {
				ASX += num * TX;
				ADX += num * TX;
				assert(engineTime >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move VRAM -> VRAM (Y direction only).
  */
template<typename Mode>
void VDPCmdEngine::startYmmm(EmuTime::param time)
{
	vram.cmdReadWindow .setMask(0x3FFFF, ~0u << 18, time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, 512, ARG);
		// large enough so that it gets clipped
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	ADX = DX;
	ANX = tmpNX;
	nextAccessSlot(time);
	calcFinishTime(tmpNX, tmpNY, 24 + 40);
	phase = 0;
}

template<typename Mode>
void VDPCmdEngine::executeYmmm(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, 512, ARG);
		// large enough so that it gets clipped
	unsigned tmpNY = clipNY_2(SY, DY, NY, ARG);
	int TX = (ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(ADX, 512, ARG);

	// TODO does this use MXD for both read and write?
	//  it says so in the datasheet, but it seems illogical
	//  OTOH YMMM also uses DX for both read and write
	bool dstExt = (ARG & MXD) != 0;
	bool doPset  = !dstExt || hasExtendedVRAM;
	auto calculator = getSlotCalculator(limit);

	switch (phase) {
	case 0:
loop:		if (calculator.limitReached()) [[unlikely]] { phase = 0; break; }
		if (doPset) [[likely]] {
			tmpSrc = vram.cmdReadWindow.readNP(
			       Mode::addressOf(ADX, SY, dstExt));
		}
		calculator.next(Delta::D24);
		[[fallthrough]];
	case 1:
		if (calculator.limitReached()) [[unlikely]] { phase = 1; break; }
		if (doPset) [[likely]] {
			vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
			              tmpSrc, calculator.getTime());
		}
		ADX += TX;
		if (--ANX == 0) {
			// note: going to the next line does not take extra time
			SY += TY; DY += TY; --NY;
			ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(calculator.getTime());
				break;
			}
		}
		calculator.next(Delta::D40);
		goto loop;
	default:
		UNREACHABLE;
	}
	engineTime = calculator.getTime();
	calcFinishTime(tmpNX, tmpNY, 24 + 40);

	/*
	if (dstExt) [[unlikely]] {
		bool doPset  = !dstExt || hasExtendedVRAM;
		while (engineTime < limit) {
			if (doPset) [[likely]] {
				byte p = vram.cmdReadWindow.readNP(
					      Mode::addressOf(ADX, SY, dstExt));
				vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
					      p, engineTime);
			}
			engineTime += delta;
			ADX += TX;
			if (--ANX == 0) {
				SY += TY; DY += TY; --NY;
				ADX = DX; ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			}
		}
	} else {
		// fast-path, no extended VRAM
		while (engineTime < limit) {
			typename Mode::IncrByteAddr srcAddr(ADX, SY, TX);
			typename Mode::IncrByteAddr dstAddr(ADX, DY, TX);
			EmuDuration dur = limit - engineTime;
			unsigned num = (delta != EmuDuration::zero())
			             ? std::min(dur.divUp(delta), ANX)
			             : ANX;
			for (auto i : xrange(num)) {
				byte p = vram.cmdReadWindow.readNP(srcAddr.getAddr());
				vram.cmdWrite(dstAddr.getAddr(), p, engineTime);
				engineTime += delta;
				srcAddr.step(TX);
				dstAddr.step(TX);
			}
			ANX -= num;
			if (ANX == 0) {
				SY += TY;
				DY += TY;
				NY -= 1;
				ADX = DX;
				ANX = tmpNX;
				if (--tmpNY == 0) {
					commandDone(engineTime);
					break;
				}
			} else {
				ADX += num * TX;
				assert(engineTime >= limit);
				break;
			}
		}
	}
	*/
}

/** High-speed move CPU -> VRAM.
  */
template<typename Mode>
void VDPCmdEngine::startHmmc(EmuTime::param time)
{
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.setMask(0x3FFFF, ~0u << 18, time);
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, NX, ARG);
	ADX = DX;
	ANX = tmpNX;
	setStatusChangeTime(EmuTime::zero());
	// do not set 'transfer = true', see startLmmc()
	status |= 0x80;
	nextAccessSlot(time);
}

template<typename Mode>
void VDPCmdEngine::executeHmmc(EmuTime::param limit)
{
	NY &= 1023;
	unsigned tmpNX = clipNX_1_byte<Mode>(DX, NX, ARG);
	unsigned tmpNY = clipNY_1(DY, NY, ARG);
	int TX = (ARG & DIX)
		? -Mode::PIXELS_PER_BYTE : Mode::PIXELS_PER_BYTE;
	int TY = (ARG & DIY) ? -1 : 1;
	ANX = clipNX_1_byte<Mode>(
		ADX, ANX << Mode::PIXELS_PER_BYTE_SHIFT, ARG);
	bool dstExt = (ARG & MXD) != 0;
	bool doPset = !dstExt || hasExtendedVRAM;

	if (transfer) {
		// TODO: timing is inaccurate. We should
		//  - wait for a byte
		//  - on the next access slot write that byte
		if (doPset) [[likely]] {
			vram.cmdWrite(Mode::addressOf(ADX, DY, dstExt),
			              COL, limit);
		}
		transfer = false;

		ADX += TX; --ANX;
		if (ANX == 0) {
			DY += TY; --NY;
			ADX = DX; ANX = tmpNX;
			if (--tmpNY == 0) {
				commandDone(limit);
			}
		}
	}
	nextAccessSlot(limit); // inaccurate, but avoid assert
}


VDPCmdEngine::VDPCmdEngine(VDP& vdp_, CommandController& commandController)
	: vdp(vdp_), vram(vdp.getVRAM())
	, cmdTraceSetting(
		commandController, vdp_.getName() == "VDP" ? "vdpcmdtrace" :
		vdp_.getName() + " vdpcmdtrace", "VDP command tracing on/off",
		false)
	, cmdInProgressCallback(
		commandController, vdp_.getName() == "VDP" ?
		"vdpcmdinprogress_callback" : vdp_.getName() +
		" vdpcmdinprogress_callback",
	        "Tcl proc to call when a write to the VDP command engine is "
		"detected while the previous command is still in progress.",
		"",
		Setting::Save::YES)
	, executingProbe(
		vdp_.getMotherBoard().getDebugger(),
		strCat(vdp.getName(), '.', "commandExecuting"),
		"Is the V99x8 VDP is currently executing a command",
		false)
	, hasExtendedVRAM(vram.getSize() == (192 * 1024))
{
}

void VDPCmdEngine::reset(EmuTime::param time)
{
	for (int i = 14; i >= 0; --i) { // start with ABORT
		setCmdReg(byte(i), 0, time);
	}
	status = 0;
	scrMode = -1;

	updateDisplayMode(vdp.getDisplayMode(), vdp.getCmdBit(), time);
}

void VDPCmdEngine::setCmdReg(byte index, byte value, EmuTime::param time)
{
	sync(time);
	if (CMD && (index != 12)) {
		cmdInProgressCallback.execute(index, value);
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
		if (!CMD) status &= 0x7F;
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

byte VDPCmdEngine::peekCmdReg(byte index) const
{
	switch (index) {
	case 0x00: return narrow_cast<byte>(SX & 0xFF);
	case 0x01: return narrow_cast<byte>(SX >> 8);
	case 0x02: return narrow_cast<byte>(SY & 0xFF);
	case 0x03: return narrow_cast<byte>(SY >> 8);

	case 0x04: return narrow_cast<byte>(DX & 0xFF);
	case 0x05: return narrow_cast<byte>(DX >> 8);
	case 0x06: return narrow_cast<byte>(DY & 0xFF);
	case 0x07: return narrow_cast<byte>(DY >> 8);

	case 0x08: return narrow_cast<byte>(NX & 0xFF);
	case 0x09: return narrow_cast<byte>(NX >> 8);
	case 0x0A: return narrow_cast<byte>(NY & 0xFF);
	case 0x0B: return narrow_cast<byte>(NY >> 8);

	case 0x0C: return COL;
	case 0x0D: return ARG;
	case 0x0E: return CMD;
	default: UNREACHABLE;
	}
}

void VDPCmdEngine::updateDisplayMode(DisplayMode mode, bool cmdBit, EmuTime::param time)
{
	int newScrMode = [&] {
		switch (mode.getBase()) {
		case DisplayMode::GRAPHIC4:
			return 0;
		case DisplayMode::GRAPHIC5:
			return 1;
		case DisplayMode::GRAPHIC6:
			return 2;
		case DisplayMode::GRAPHIC7:
			return 3;
		default:
			if (cmdBit) {
				return 4; // like GRAPHIC7, but non-planar
				          // TODO timing might be different
			} else {
				return -1; // no commands
			}
		}
	}();

	if (newScrMode != scrMode) {
		sync(time);
		if (CMD) {
			// VDP mode switch while command in progress
			if (newScrMode == -1) {
				// TODO: For now abort cmd in progress,
				//       later find out what really happens.
				//       At least CE remains high for a while,
				//       but it is not yet clear what happens in VRAM.
				commandDone(time);
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

	// store a copy of the start registers
	lastSX = SX; lastSY = SY;
	lastDX = DX; lastDY = DY;
	lastNX = NX; lastNY = NY;
	lastCOL = COL; lastARG = ARG; lastCMD = CMD;

	if (cmdTraceSetting.getBoolean()) {
		reportVdpCommand();
	}

	// Start command.
	status |= 0x01;
	executingProbe = true;

	switch ((scrMode << 4) | (CMD >> 4)) {
	case 0x00: case 0x10: case 0x20: case 0x30: case 0x40:
	case 0x01: case 0x11: case 0x21: case 0x31: case 0x41:
	case 0x02: case 0x12: case 0x22: case 0x32: case 0x42:
	case 0x03: case 0x13: case 0x23: case 0x33: case 0x43:
		startAbrt(time); break;

	case 0x04: case 0x14: case 0x24: case 0x34: case 0x44:
		startPoint(time); break;
	case 0x05: case 0x15: case 0x25: case 0x35: case 0x45:
		startPset(time); break;
	case 0x06: case 0x16: case 0x26: case 0x36: case 0x46:
		startSrch(time); break;
	case 0x07: case 0x17: case 0x27: case 0x37: case 0x47:
		startLine(time); break;

	case 0x08: startLmmv<Graphic4Mode >(time); break;
	case 0x18: startLmmv<Graphic5Mode >(time); break;
	case 0x28: startLmmv<Graphic6Mode >(time); break;
	case 0x38: startLmmv<Graphic7Mode >(time); break;
	case 0x48: startLmmv<NonBitmapMode>(time); break;

	case 0x09: startLmmm<Graphic4Mode >(time); break;
	case 0x19: startLmmm<Graphic5Mode >(time); break;
	case 0x29: startLmmm<Graphic6Mode >(time); break;
	case 0x39: startLmmm<Graphic7Mode >(time); break;
	case 0x49: startLmmm<NonBitmapMode>(time); break;

	case 0x0A: startLmcm<Graphic4Mode >(time); break;
	case 0x1A: startLmcm<Graphic5Mode >(time); break;
	case 0x2A: startLmcm<Graphic6Mode >(time); break;
	case 0x3A: startLmcm<Graphic7Mode >(time); break;
	case 0x4A: startLmcm<NonBitmapMode>(time); break;

	case 0x0B: startLmmc<Graphic4Mode >(time); break;
	case 0x1B: startLmmc<Graphic5Mode >(time); break;
	case 0x2B: startLmmc<Graphic6Mode >(time); break;
	case 0x3B: startLmmc<Graphic7Mode >(time); break;
	case 0x4B: startLmmc<NonBitmapMode>(time); break;

	case 0x0C: startHmmv<Graphic4Mode >(time); break;
	case 0x1C: startHmmv<Graphic5Mode >(time); break;
	case 0x2C: startHmmv<Graphic6Mode >(time); break;
	case 0x3C: startHmmv<Graphic7Mode >(time); break;
	case 0x4C: startHmmv<NonBitmapMode>(time); break;

	case 0x0D: startHmmm<Graphic4Mode >(time); break;
	case 0x1D: startHmmm<Graphic5Mode >(time); break;
	case 0x2D: startHmmm<Graphic6Mode >(time); break;
	case 0x3D: startHmmm<Graphic7Mode >(time); break;
	case 0x4D: startHmmm<NonBitmapMode>(time); break;

	case 0x0E: startYmmm<Graphic4Mode >(time); break;
	case 0x1E: startYmmm<Graphic5Mode >(time); break;
	case 0x2E: startYmmm<Graphic6Mode >(time); break;
	case 0x3E: startYmmm<Graphic7Mode >(time); break;
	case 0x4E: startYmmm<NonBitmapMode>(time); break;

	case 0x0F: startHmmc<Graphic4Mode >(time); break;
	case 0x1F: startHmmc<Graphic5Mode >(time); break;
	case 0x2F: startHmmc<Graphic6Mode >(time); break;
	case 0x3F: startHmmc<Graphic7Mode >(time); break;
	case 0x4F: startHmmc<NonBitmapMode>(time); break;

	default: UNREACHABLE;
	}
}

void VDPCmdEngine::sync2(EmuTime::param time)
{
	switch ((scrMode << 8) | CMD) {
	case 0x000: case 0x100: case 0x200: case 0x300: case 0x400:
	case 0x001: case 0x101: case 0x201: case 0x301: case 0x401:
	case 0x002: case 0x102: case 0x202: case 0x302: case 0x402:
	case 0x003: case 0x103: case 0x203: case 0x303: case 0x403:
	case 0x004: case 0x104: case 0x204: case 0x304: case 0x404:
	case 0x005: case 0x105: case 0x205: case 0x305: case 0x405:
	case 0x006: case 0x106: case 0x206: case 0x306: case 0x406:
	case 0x007: case 0x107: case 0x207: case 0x307: case 0x407:
	case 0x008: case 0x108: case 0x208: case 0x308: case 0x408:
	case 0x009: case 0x109: case 0x209: case 0x309: case 0x409:
	case 0x00A: case 0x10A: case 0x20A: case 0x30A: case 0x40A:
	case 0x00B: case 0x10B: case 0x20B: case 0x30B: case 0x40B:
	case 0x00C: case 0x10C: case 0x20C: case 0x30C: case 0x40C:
	case 0x00D: case 0x10D: case 0x20D: case 0x30D: case 0x40D:
	case 0x00E: case 0x10E: case 0x20E: case 0x30E: case 0x40E:
	case 0x00F: case 0x10F: case 0x20F: case 0x30F: case 0x40F:
	case 0x010: case 0x110: case 0x210: case 0x310: case 0x410:
	case 0x011: case 0x111: case 0x211: case 0x311: case 0x411:
	case 0x012: case 0x112: case 0x212: case 0x312: case 0x412:
	case 0x013: case 0x113: case 0x213: case 0x313: case 0x413:
	case 0x014: case 0x114: case 0x214: case 0x314: case 0x414:
	case 0x015: case 0x115: case 0x215: case 0x315: case 0x415:
	case 0x016: case 0x116: case 0x216: case 0x316: case 0x416:
	case 0x017: case 0x117: case 0x217: case 0x317: case 0x417:
	case 0x018: case 0x118: case 0x218: case 0x318: case 0x418:
	case 0x019: case 0x119: case 0x219: case 0x319: case 0x419:
	case 0x01A: case 0x11A: case 0x21A: case 0x31A: case 0x41A:
	case 0x01B: case 0x11B: case 0x21B: case 0x31B: case 0x41B:
	case 0x01C: case 0x11C: case 0x21C: case 0x31C: case 0x41C:
	case 0x01D: case 0x11D: case 0x21D: case 0x31D: case 0x41D:
	case 0x01E: case 0x11E: case 0x21E: case 0x31E: case 0x41E:
	case 0x01F: case 0x11F: case 0x21F: case 0x31F: case 0x41F:
	case 0x020: case 0x120: case 0x220: case 0x320: case 0x420:
	case 0x021: case 0x121: case 0x221: case 0x321: case 0x421:
	case 0x022: case 0x122: case 0x222: case 0x322: case 0x422:
	case 0x023: case 0x123: case 0x223: case 0x323: case 0x423:
	case 0x024: case 0x124: case 0x224: case 0x324: case 0x424:
	case 0x025: case 0x125: case 0x225: case 0x325: case 0x425:
	case 0x026: case 0x126: case 0x226: case 0x326: case 0x426:
	case 0x027: case 0x127: case 0x227: case 0x327: case 0x427:
	case 0x028: case 0x128: case 0x228: case 0x328: case 0x428:
	case 0x029: case 0x129: case 0x229: case 0x329: case 0x429:
	case 0x02A: case 0x12A: case 0x22A: case 0x32A: case 0x42A:
	case 0x02B: case 0x12B: case 0x22B: case 0x32B: case 0x42B:
	case 0x02C: case 0x12C: case 0x22C: case 0x32C: case 0x42C:
	case 0x02D: case 0x12D: case 0x22D: case 0x32D: case 0x42D:
	case 0x02E: case 0x12E: case 0x22E: case 0x32E: case 0x42E:
	case 0x02F: case 0x12F: case 0x22F: case 0x32F: case 0x42F:
	case 0x030: case 0x130: case 0x230: case 0x330: case 0x430:
	case 0x031: case 0x131: case 0x231: case 0x331: case 0x431:
	case 0x032: case 0x132: case 0x232: case 0x332: case 0x432:
	case 0x033: case 0x133: case 0x233: case 0x333: case 0x433:
	case 0x034: case 0x134: case 0x234: case 0x334: case 0x434:
	case 0x035: case 0x135: case 0x235: case 0x335: case 0x435:
	case 0x036: case 0x136: case 0x236: case 0x336: case 0x436:
	case 0x037: case 0x137: case 0x237: case 0x337: case 0x437:
	case 0x038: case 0x138: case 0x238: case 0x338: case 0x438:
	case 0x039: case 0x139: case 0x239: case 0x339: case 0x439:
	case 0x03A: case 0x13A: case 0x23A: case 0x33A: case 0x43A:
	case 0x03B: case 0x13B: case 0x23B: case 0x33B: case 0x43B:
	case 0x03C: case 0x13C: case 0x23C: case 0x33C: case 0x43C:
	case 0x03D: case 0x13D: case 0x23D: case 0x33D: case 0x43D:
	case 0x03E: case 0x13E: case 0x23E: case 0x33E: case 0x43E:
	case 0x03F: case 0x13F: case 0x23F: case 0x33F: case 0x43F:
		UNREACHABLE;

	case 0x040: case 0x041: case 0x042: case 0x043:
	case 0x044: case 0x045: case 0x046: case 0x047:
	case 0x048: case 0x049: case 0x04A: case 0x04B:
	case 0x04C: case 0x04D: case 0x04E: case 0x04F:
		executePoint<Graphic4Mode>(time); break;
	case 0x140: case 0x141: case 0x142: case 0x143:
	case 0x144: case 0x145: case 0x146: case 0x147:
	case 0x148: case 0x149: case 0x14A: case 0x14B:
	case 0x14C: case 0x14D: case 0x14E: case 0x14F:
		executePoint<Graphic5Mode>(time); break;
	case 0x240: case 0x241: case 0x242: case 0x243:
	case 0x244: case 0x245: case 0x246: case 0x247:
	case 0x248: case 0x249: case 0x24A: case 0x24B:
	case 0x24C: case 0x24D: case 0x24E: case 0x24F:
		executePoint<Graphic6Mode>(time); break;
	case 0x340: case 0x341: case 0x342: case 0x343:
	case 0x344: case 0x345: case 0x346: case 0x347:
	case 0x348: case 0x349: case 0x34A: case 0x34B:
	case 0x34C: case 0x34D: case 0x34E: case 0x34F:
		executePoint<Graphic7Mode>(time); break;
	case 0x440: case 0x441: case 0x442: case 0x443:
	case 0x444: case 0x445: case 0x446: case 0x447:
	case 0x448: case 0x449: case 0x44A: case 0x44B:
	case 0x44C: case 0x44D: case 0x44E: case 0x44F:
		executePoint<NonBitmapMode>(time); break;

	case 0x050: executePset<Graphic4Mode,  ImpOp>(time); break;
	case 0x051: executePset<Graphic4Mode,  AndOp>(time); break;
	case 0x052: executePset<Graphic4Mode,  OrOp >(time); break;
	case 0x053: executePset<Graphic4Mode,  XorOp>(time); break;
	case 0x054: executePset<Graphic4Mode,  NotOp>(time); break;
	case 0x058: executePset<Graphic4Mode, TImpOp>(time); break;
	case 0x059: executePset<Graphic4Mode, TAndOp>(time); break;
	case 0x05A: executePset<Graphic4Mode, TOrOp >(time); break;
	case 0x05B: executePset<Graphic4Mode, TXorOp>(time); break;
	case 0x05C: executePset<Graphic4Mode, TNotOp>(time); break;
	case 0x055: case 0x056: case 0x057: case 0x05D: case 0x05E: case 0x05F:
		executePset<Graphic4Mode, DummyOp>(time); break;
	case 0x150: executePset<Graphic5Mode,  ImpOp>(time); break;
	case 0x151: executePset<Graphic5Mode,  AndOp>(time); break;
	case 0x152: executePset<Graphic5Mode,  OrOp >(time); break;
	case 0x153: executePset<Graphic5Mode,  XorOp>(time); break;
	case 0x154: executePset<Graphic5Mode,  NotOp>(time); break;
	case 0x158: executePset<Graphic5Mode, TImpOp>(time); break;
	case 0x159: executePset<Graphic5Mode, TAndOp>(time); break;
	case 0x15A: executePset<Graphic5Mode, TOrOp >(time); break;
	case 0x15B: executePset<Graphic5Mode, TXorOp>(time); break;
	case 0x15C: executePset<Graphic5Mode, TNotOp>(time); break;
	case 0x155: case 0x156: case 0x157: case 0x15D: case 0x15E: case 0x15F:
		executePset<Graphic5Mode, DummyOp>(time); break;
	case 0x250: executePset<Graphic6Mode,  ImpOp>(time); break;
	case 0x251: executePset<Graphic6Mode,  AndOp>(time); break;
	case 0x252: executePset<Graphic6Mode,  OrOp >(time); break;
	case 0x253: executePset<Graphic6Mode,  XorOp>(time); break;
	case 0x254: executePset<Graphic6Mode,  NotOp>(time); break;
	case 0x258: executePset<Graphic6Mode, TImpOp>(time); break;
	case 0x259: executePset<Graphic6Mode, TAndOp>(time); break;
	case 0x25A: executePset<Graphic6Mode, TOrOp >(time); break;
	case 0x25B: executePset<Graphic6Mode, TXorOp>(time); break;
	case 0x25C: executePset<Graphic6Mode, TNotOp>(time); break;
	case 0x255: case 0x256: case 0x257: case 0x25D: case 0x25E: case 0x25F:
		executePset<Graphic6Mode, DummyOp>(time); break;
	case 0x350: executePset<Graphic7Mode,  ImpOp>(time); break;
	case 0x351: executePset<Graphic7Mode,  AndOp>(time); break;
	case 0x352: executePset<Graphic7Mode,  OrOp >(time); break;
	case 0x353: executePset<Graphic7Mode,  XorOp>(time); break;
	case 0x354: executePset<Graphic7Mode,  NotOp>(time); break;
	case 0x358: executePset<Graphic7Mode, TImpOp>(time); break;
	case 0x359: executePset<Graphic7Mode, TAndOp>(time); break;
	case 0x35A: executePset<Graphic7Mode, TOrOp >(time); break;
	case 0x35B: executePset<Graphic7Mode, TXorOp>(time); break;
	case 0x35C: executePset<Graphic7Mode, TNotOp>(time); break;
	case 0x355: case 0x356: case 0x357: case 0x35D: case 0x35E: case 0x35F:
		executePset<Graphic7Mode, DummyOp>(time); break;
	case 0x450: executePset<NonBitmapMode,  ImpOp>(time); break;
	case 0x451: executePset<NonBitmapMode,  AndOp>(time); break;
	case 0x452: executePset<NonBitmapMode,  OrOp >(time); break;
	case 0x453: executePset<NonBitmapMode,  XorOp>(time); break;
	case 0x454: executePset<NonBitmapMode,  NotOp>(time); break;
	case 0x458: executePset<NonBitmapMode, TImpOp>(time); break;
	case 0x459: executePset<NonBitmapMode, TAndOp>(time); break;
	case 0x45A: executePset<NonBitmapMode, TOrOp >(time); break;
	case 0x45B: executePset<NonBitmapMode, TXorOp>(time); break;
	case 0x45C: executePset<NonBitmapMode, TNotOp>(time); break;
	case 0x455: case 0x456: case 0x457: case 0x45D: case 0x45E: case 0x45F:
		executePset<NonBitmapMode, DummyOp>(time); break;

	case 0x060: case 0x061: case 0x062: case 0x063:
	case 0x064: case 0x065: case 0x066: case 0x067:
	case 0x068: case 0x069: case 0x06A: case 0x06B:
	case 0x06C: case 0x06D: case 0x06E: case 0x06F:
		executeSrch<Graphic4Mode>(time); break;
	case 0x160: case 0x161: case 0x162: case 0x163:
	case 0x164: case 0x165: case 0x166: case 0x167:
	case 0x168: case 0x169: case 0x16A: case 0x16B:
	case 0x16C: case 0x16D: case 0x16E: case 0x16F:
		executeSrch<Graphic5Mode>(time); break;
	case 0x260: case 0x261: case 0x262: case 0x263:
	case 0x264: case 0x265: case 0x266: case 0x267:
	case 0x268: case 0x269: case 0x26A: case 0x26B:
	case 0x26C: case 0x26D: case 0x26E: case 0x26F:
		executeSrch<Graphic6Mode>(time); break;
	case 0x360: case 0x361: case 0x362: case 0x363:
	case 0x364: case 0x365: case 0x366: case 0x367:
	case 0x368: case 0x369: case 0x36A: case 0x36B:
	case 0x36C: case 0x36D: case 0x36E: case 0x36F:
		executeSrch<Graphic7Mode>(time); break;
	case 0x460: case 0x461: case 0x462: case 0x463:
	case 0x464: case 0x465: case 0x466: case 0x467:
	case 0x468: case 0x469: case 0x46A: case 0x46B:
	case 0x46C: case 0x46D: case 0x46E: case 0x46F:
		executeSrch<NonBitmapMode>(time); break;

	case 0x070: executeLine<Graphic4Mode,  ImpOp>(time); break;
	case 0x071: executeLine<Graphic4Mode,  AndOp>(time); break;
	case 0x072: executeLine<Graphic4Mode,  OrOp >(time); break;
	case 0x073: executeLine<Graphic4Mode,  XorOp>(time); break;
	case 0x074: executeLine<Graphic4Mode,  NotOp>(time); break;
	case 0x078: executeLine<Graphic4Mode, TImpOp>(time); break;
	case 0x079: executeLine<Graphic4Mode, TAndOp>(time); break;
	case 0x07A: executeLine<Graphic4Mode, TOrOp >(time); break;
	case 0x07B: executeLine<Graphic4Mode, TXorOp>(time); break;
	case 0x07C: executeLine<Graphic4Mode, TNotOp>(time); break;
	case 0x075: case 0x076: case 0x077: case 0x07D: case 0x07E: case 0x07F:
		executeLine<Graphic4Mode, DummyOp>(time); break;
	case 0x170: executeLine<Graphic5Mode,  ImpOp>(time); break;
	case 0x171: executeLine<Graphic5Mode,  AndOp>(time); break;
	case 0x172: executeLine<Graphic5Mode,  OrOp >(time); break;
	case 0x173: executeLine<Graphic5Mode,  XorOp>(time); break;
	case 0x174: executeLine<Graphic5Mode,  NotOp>(time); break;
	case 0x178: executeLine<Graphic5Mode, TImpOp>(time); break;
	case 0x179: executeLine<Graphic5Mode, TAndOp>(time); break;
	case 0x17A: executeLine<Graphic5Mode, TOrOp >(time); break;
	case 0x17B: executeLine<Graphic5Mode, TXorOp>(time); break;
	case 0x17C: executeLine<Graphic5Mode, TNotOp>(time); break;
	case 0x175: case 0x176: case 0x177: case 0x17D: case 0x17E: case 0x17F:
		executeLine<Graphic5Mode, DummyOp>(time); break;
	case 0x270: executeLine<Graphic6Mode,  ImpOp>(time); break;
	case 0x271: executeLine<Graphic6Mode,  AndOp>(time); break;
	case 0x272: executeLine<Graphic6Mode,  OrOp >(time); break;
	case 0x273: executeLine<Graphic6Mode,  XorOp>(time); break;
	case 0x274: executeLine<Graphic6Mode,  NotOp>(time); break;
	case 0x278: executeLine<Graphic6Mode, TImpOp>(time); break;
	case 0x279: executeLine<Graphic6Mode, TAndOp>(time); break;
	case 0x27A: executeLine<Graphic6Mode, TOrOp >(time); break;
	case 0x27B: executeLine<Graphic6Mode, TXorOp>(time); break;
	case 0x27C: executeLine<Graphic6Mode, TNotOp>(time); break;
	case 0x275: case 0x276: case 0x277: case 0x27D: case 0x27E: case 0x27F:
		executeLine<Graphic6Mode, DummyOp>(time); break;
	case 0x370: executeLine<Graphic7Mode,  ImpOp>(time); break;
	case 0x371: executeLine<Graphic7Mode,  AndOp>(time); break;
	case 0x372: executeLine<Graphic7Mode,  OrOp >(time); break;
	case 0x373: executeLine<Graphic7Mode,  XorOp>(time); break;
	case 0x374: executeLine<Graphic7Mode,  NotOp>(time); break;
	case 0x378: executeLine<Graphic7Mode, TImpOp>(time); break;
	case 0x379: executeLine<Graphic7Mode, TAndOp>(time); break;
	case 0x37A: executeLine<Graphic7Mode, TOrOp >(time); break;
	case 0x37B: executeLine<Graphic7Mode, TXorOp>(time); break;
	case 0x37C: executeLine<Graphic7Mode, TNotOp>(time); break;
	case 0x375: case 0x376: case 0x377: case 0x37D: case 0x37E: case 0x37F:
		executeLine<Graphic7Mode, DummyOp>(time); break;
	case 0x470: executeLine<NonBitmapMode,  ImpOp>(time); break;
	case 0x471: executeLine<NonBitmapMode,  AndOp>(time); break;
	case 0x472: executeLine<NonBitmapMode,  OrOp >(time); break;
	case 0x473: executeLine<NonBitmapMode,  XorOp>(time); break;
	case 0x474: executeLine<NonBitmapMode,  NotOp>(time); break;
	case 0x478: executeLine<NonBitmapMode, TImpOp>(time); break;
	case 0x479: executeLine<NonBitmapMode, TAndOp>(time); break;
	case 0x47A: executeLine<NonBitmapMode, TOrOp >(time); break;
	case 0x47B: executeLine<NonBitmapMode, TXorOp>(time); break;
	case 0x47C: executeLine<NonBitmapMode, TNotOp>(time); break;
	case 0x475: case 0x476: case 0x477: case 0x47D: case 0x47E: case 0x47F:
		executeLine<NonBitmapMode, DummyOp>(time); break;

	case 0x080: executeLmmv<Graphic4Mode,  ImpOp>(time); break;
	case 0x081: executeLmmv<Graphic4Mode,  AndOp>(time); break;
	case 0x082: executeLmmv<Graphic4Mode,  OrOp >(time); break;
	case 0x083: executeLmmv<Graphic4Mode,  XorOp>(time); break;
	case 0x084: executeLmmv<Graphic4Mode,  NotOp>(time); break;
	case 0x088: executeLmmv<Graphic4Mode, TImpOp>(time); break;
	case 0x089: executeLmmv<Graphic4Mode, TAndOp>(time); break;
	case 0x08A: executeLmmv<Graphic4Mode, TOrOp >(time); break;
	case 0x08B: executeLmmv<Graphic4Mode, TXorOp>(time); break;
	case 0x08C: executeLmmv<Graphic4Mode, TNotOp>(time); break;
	case 0x085: case 0x086: case 0x087: case 0x08D: case 0x08E: case 0x08F:
		executeLmmv<Graphic4Mode, DummyOp>(time); break;
	case 0x180: executeLmmv<Graphic5Mode,  ImpOp>(time); break;
	case 0x181: executeLmmv<Graphic5Mode,  AndOp>(time); break;
	case 0x182: executeLmmv<Graphic5Mode,  OrOp >(time); break;
	case 0x183: executeLmmv<Graphic5Mode,  XorOp>(time); break;
	case 0x184: executeLmmv<Graphic5Mode,  NotOp>(time); break;
	case 0x188: executeLmmv<Graphic5Mode, TImpOp>(time); break;
	case 0x189: executeLmmv<Graphic5Mode, TAndOp>(time); break;
	case 0x18A: executeLmmv<Graphic5Mode, TOrOp >(time); break;
	case 0x18B: executeLmmv<Graphic5Mode, TXorOp>(time); break;
	case 0x18C: executeLmmv<Graphic5Mode, TNotOp>(time); break;
	case 0x185: case 0x186: case 0x187: case 0x18D: case 0x18E: case 0x18F:
		executeLmmv<Graphic5Mode, DummyOp>(time); break;
	case 0x280: executeLmmv<Graphic6Mode,  ImpOp>(time); break;
	case 0x281: executeLmmv<Graphic6Mode,  AndOp>(time); break;
	case 0x282: executeLmmv<Graphic6Mode,  OrOp >(time); break;
	case 0x283: executeLmmv<Graphic6Mode,  XorOp>(time); break;
	case 0x284: executeLmmv<Graphic6Mode,  NotOp>(time); break;
	case 0x288: executeLmmv<Graphic6Mode, TImpOp>(time); break;
	case 0x289: executeLmmv<Graphic6Mode, TAndOp>(time); break;
	case 0x28A: executeLmmv<Graphic6Mode, TOrOp >(time); break;
	case 0x28B: executeLmmv<Graphic6Mode, TXorOp>(time); break;
	case 0x28C: executeLmmv<Graphic6Mode, TNotOp>(time); break;
	case 0x285: case 0x286: case 0x287: case 0x28D: case 0x28E: case 0x28F:
		executeLmmv<Graphic6Mode, DummyOp>(time); break;
	case 0x380: executeLmmv<Graphic7Mode,  ImpOp>(time); break;
	case 0x381: executeLmmv<Graphic7Mode,  AndOp>(time); break;
	case 0x382: executeLmmv<Graphic7Mode,  OrOp >(time); break;
	case 0x383: executeLmmv<Graphic7Mode,  XorOp>(time); break;
	case 0x384: executeLmmv<Graphic7Mode,  NotOp>(time); break;
	case 0x388: executeLmmv<Graphic7Mode, TImpOp>(time); break;
	case 0x389: executeLmmv<Graphic7Mode, TAndOp>(time); break;
	case 0x38A: executeLmmv<Graphic7Mode, TOrOp >(time); break;
	case 0x38B: executeLmmv<Graphic7Mode, TXorOp>(time); break;
	case 0x38C: executeLmmv<Graphic7Mode, TNotOp>(time); break;
	case 0x385: case 0x386: case 0x387: case 0x38D: case 0x38E: case 0x38F:
		executeLmmv<Graphic7Mode, DummyOp>(time); break;
	case 0x480: executeLmmv<NonBitmapMode,  ImpOp>(time); break;
	case 0x481: executeLmmv<NonBitmapMode,  AndOp>(time); break;
	case 0x482: executeLmmv<NonBitmapMode,  OrOp >(time); break;
	case 0x483: executeLmmv<NonBitmapMode,  XorOp>(time); break;
	case 0x484: executeLmmv<NonBitmapMode,  NotOp>(time); break;
	case 0x488: executeLmmv<NonBitmapMode, TImpOp>(time); break;
	case 0x489: executeLmmv<NonBitmapMode, TAndOp>(time); break;
	case 0x48A: executeLmmv<NonBitmapMode, TOrOp >(time); break;
	case 0x48B: executeLmmv<NonBitmapMode, TXorOp>(time); break;
	case 0x48C: executeLmmv<NonBitmapMode, TNotOp>(time); break;
	case 0x485: case 0x486: case 0x487: case 0x48D: case 0x48E: case 0x48F:
		executeLmmv<NonBitmapMode, DummyOp>(time); break;

	case 0x090: executeLmmm<Graphic4Mode,  ImpOp>(time); break;
	case 0x091: executeLmmm<Graphic4Mode,  AndOp>(time); break;
	case 0x092: executeLmmm<Graphic4Mode,  OrOp >(time); break;
	case 0x093: executeLmmm<Graphic4Mode,  XorOp>(time); break;
	case 0x094: executeLmmm<Graphic4Mode,  NotOp>(time); break;
	case 0x098: executeLmmm<Graphic4Mode, TImpOp>(time); break;
	case 0x099: executeLmmm<Graphic4Mode, TAndOp>(time); break;
	case 0x09A: executeLmmm<Graphic4Mode, TOrOp >(time); break;
	case 0x09B: executeLmmm<Graphic4Mode, TXorOp>(time); break;
	case 0x09C: executeLmmm<Graphic4Mode, TNotOp>(time); break;
	case 0x095: case 0x096: case 0x097: case 0x09D: case 0x09E: case 0x09F:
		executeLmmm<Graphic4Mode, DummyOp>(time); break;
	case 0x190: executeLmmm<Graphic5Mode,  ImpOp>(time); break;
	case 0x191: executeLmmm<Graphic5Mode,  AndOp>(time); break;
	case 0x192: executeLmmm<Graphic5Mode,  OrOp >(time); break;
	case 0x193: executeLmmm<Graphic5Mode,  XorOp>(time); break;
	case 0x194: executeLmmm<Graphic5Mode,  NotOp>(time); break;
	case 0x198: executeLmmm<Graphic5Mode, TImpOp>(time); break;
	case 0x199: executeLmmm<Graphic5Mode, TAndOp>(time); break;
	case 0x19A: executeLmmm<Graphic5Mode, TOrOp >(time); break;
	case 0x19B: executeLmmm<Graphic5Mode, TXorOp>(time); break;
	case 0x19C: executeLmmm<Graphic5Mode, TNotOp>(time); break;
	case 0x195: case 0x196: case 0x197: case 0x19D: case 0x19E: case 0x19F:
		executeLmmm<Graphic5Mode, DummyOp>(time); break;
	case 0x290: executeLmmm<Graphic6Mode,  ImpOp>(time); break;
	case 0x291: executeLmmm<Graphic6Mode,  AndOp>(time); break;
	case 0x292: executeLmmm<Graphic6Mode,  OrOp >(time); break;
	case 0x293: executeLmmm<Graphic6Mode,  XorOp>(time); break;
	case 0x294: executeLmmm<Graphic6Mode,  NotOp>(time); break;
	case 0x298: executeLmmm<Graphic6Mode, TImpOp>(time); break;
	case 0x299: executeLmmm<Graphic6Mode, TAndOp>(time); break;
	case 0x29A: executeLmmm<Graphic6Mode, TOrOp >(time); break;
	case 0x29B: executeLmmm<Graphic6Mode, TXorOp>(time); break;
	case 0x29C: executeLmmm<Graphic6Mode, TNotOp>(time); break;
	case 0x295: case 0x296: case 0x297: case 0x29D: case 0x29E: case 0x29F:
		executeLmmm<Graphic6Mode, DummyOp>(time); break;
	case 0x390: executeLmmm<Graphic7Mode,  ImpOp>(time); break;
	case 0x391: executeLmmm<Graphic7Mode,  AndOp>(time); break;
	case 0x392: executeLmmm<Graphic7Mode,  OrOp >(time); break;
	case 0x393: executeLmmm<Graphic7Mode,  XorOp>(time); break;
	case 0x394: executeLmmm<Graphic7Mode,  NotOp>(time); break;
	case 0x398: executeLmmm<Graphic7Mode, TImpOp>(time); break;
	case 0x399: executeLmmm<Graphic7Mode, TAndOp>(time); break;
	case 0x39A: executeLmmm<Graphic7Mode, TOrOp >(time); break;
	case 0x39B: executeLmmm<Graphic7Mode, TXorOp>(time); break;
	case 0x39C: executeLmmm<Graphic7Mode, TNotOp>(time); break;
	case 0x395: case 0x396: case 0x397: case 0x39D: case 0x39E: case 0x39F:
		executeLmmm<Graphic7Mode, DummyOp>(time); break;
	case 0x490: executeLmmm<NonBitmapMode,  ImpOp>(time); break;
	case 0x491: executeLmmm<NonBitmapMode,  AndOp>(time); break;
	case 0x492: executeLmmm<NonBitmapMode,  OrOp >(time); break;
	case 0x493: executeLmmm<NonBitmapMode,  XorOp>(time); break;
	case 0x494: executeLmmm<NonBitmapMode,  NotOp>(time); break;
	case 0x498: executeLmmm<NonBitmapMode, TImpOp>(time); break;
	case 0x499: executeLmmm<NonBitmapMode, TAndOp>(time); break;
	case 0x49A: executeLmmm<NonBitmapMode, TOrOp >(time); break;
	case 0x49B: executeLmmm<NonBitmapMode, TXorOp>(time); break;
	case 0x49C: executeLmmm<NonBitmapMode, TNotOp>(time); break;
	case 0x495: case 0x496: case 0x497: case 0x49D: case 0x49E: case 0x49F:
		executeLmmm<NonBitmapMode, DummyOp>(time); break;

	case 0x0A0: case 0x0A1: case 0x0A2: case 0x0A3:
	case 0x0A4: case 0x0A5: case 0x0A6: case 0x0A7:
	case 0x0A8: case 0x0A9: case 0x0AA: case 0x0AB:
	case 0x0AC: case 0x0AD: case 0x0AE: case 0x0AF:
		executeLmcm<Graphic4Mode>(time); break;
	case 0x1A0: case 0x1A1: case 0x1A2: case 0x1A3:
	case 0x1A4: case 0x1A5: case 0x1A6: case 0x1A7:
	case 0x1A8: case 0x1A9: case 0x1AA: case 0x1AB:
	case 0x1AC: case 0x1AD: case 0x1AE: case 0x1AF:
		executeLmcm<Graphic5Mode>(time); break;
	case 0x2A0: case 0x2A1: case 0x2A2: case 0x2A3:
	case 0x2A4: case 0x2A5: case 0x2A6: case 0x2A7:
	case 0x2A8: case 0x2A9: case 0x2AA: case 0x2AB:
	case 0x2AC: case 0x2AD: case 0x2AE: case 0x2AF:
		executeLmcm<Graphic6Mode>(time); break;
	case 0x3A0: case 0x3A1: case 0x3A2: case 0x3A3:
	case 0x3A4: case 0x3A5: case 0x3A6: case 0x3A7:
	case 0x3A8: case 0x3A9: case 0x3AA: case 0x3AB:
	case 0x3AC: case 0x3AD: case 0x3AE: case 0x3AF:
		executeLmcm<Graphic7Mode>(time); break;
	case 0x4A0: case 0x4A1: case 0x4A2: case 0x4A3:
	case 0x4A4: case 0x4A5: case 0x4A6: case 0x4A7:
	case 0x4A8: case 0x4A9: case 0x4AA: case 0x4AB:
	case 0x4AC: case 0x4AD: case 0x4AE: case 0x4AF:
		executeLmcm<NonBitmapMode>(time); break;

	case 0x0B0: executeLmmc<Graphic4Mode,  ImpOp>(time); break;
	case 0x0B1: executeLmmc<Graphic4Mode,  AndOp>(time); break;
	case 0x0B2: executeLmmc<Graphic4Mode,  OrOp >(time); break;
	case 0x0B3: executeLmmc<Graphic4Mode,  XorOp>(time); break;
	case 0x0B4: executeLmmc<Graphic4Mode,  NotOp>(time); break;
	case 0x0B8: executeLmmc<Graphic4Mode, TImpOp>(time); break;
	case 0x0B9: executeLmmc<Graphic4Mode, TAndOp>(time); break;
	case 0x0BA: executeLmmc<Graphic4Mode, TOrOp >(time); break;
	case 0x0BB: executeLmmc<Graphic4Mode, TXorOp>(time); break;
	case 0x0BC: executeLmmc<Graphic4Mode, TNotOp>(time); break;
	case 0x0B5: case 0x0B6: case 0x0B7: case 0x0BD: case 0x0BE: case 0x0BF:
		executeLmmc<Graphic4Mode, DummyOp>(time); break;
	case 0x1B0: executeLmmc<Graphic5Mode,  ImpOp>(time); break;
	case 0x1B1: executeLmmc<Graphic5Mode,  AndOp>(time); break;
	case 0x1B2: executeLmmc<Graphic5Mode,  OrOp >(time); break;
	case 0x1B3: executeLmmc<Graphic5Mode,  XorOp>(time); break;
	case 0x1B4: executeLmmc<Graphic5Mode,  NotOp>(time); break;
	case 0x1B8: executeLmmc<Graphic5Mode, TImpOp>(time); break;
	case 0x1B9: executeLmmc<Graphic5Mode, TAndOp>(time); break;
	case 0x1BA: executeLmmc<Graphic5Mode, TOrOp >(time); break;
	case 0x1BB: executeLmmc<Graphic5Mode, TXorOp>(time); break;
	case 0x1BC: executeLmmc<Graphic5Mode, TNotOp>(time); break;
	case 0x1B5: case 0x1B6: case 0x1B7: case 0x1BD: case 0x1BE: case 0x1BF:
		executeLmmc<Graphic5Mode, DummyOp>(time); break;
	case 0x2B0: executeLmmc<Graphic6Mode,  ImpOp>(time); break;
	case 0x2B1: executeLmmc<Graphic6Mode,  AndOp>(time); break;
	case 0x2B2: executeLmmc<Graphic6Mode,  OrOp >(time); break;
	case 0x2B3: executeLmmc<Graphic6Mode,  XorOp>(time); break;
	case 0x2B4: executeLmmc<Graphic6Mode,  NotOp>(time); break;
	case 0x2B8: executeLmmc<Graphic6Mode, TImpOp>(time); break;
	case 0x2B9: executeLmmc<Graphic6Mode, TAndOp>(time); break;
	case 0x2BA: executeLmmc<Graphic6Mode, TOrOp >(time); break;
	case 0x2BB: executeLmmc<Graphic6Mode, TXorOp>(time); break;
	case 0x2BC: executeLmmc<Graphic6Mode, TNotOp>(time); break;
	case 0x2B5: case 0x2B6: case 0x2B7: case 0x2BD: case 0x2BE: case 0x2BF:
		executeLmmc<Graphic6Mode, DummyOp>(time); break;
	case 0x3B0: executeLmmc<Graphic7Mode,  ImpOp>(time); break;
	case 0x3B1: executeLmmc<Graphic7Mode,  AndOp>(time); break;
	case 0x3B2: executeLmmc<Graphic7Mode,  OrOp >(time); break;
	case 0x3B3: executeLmmc<Graphic7Mode,  XorOp>(time); break;
	case 0x3B4: executeLmmc<Graphic7Mode,  NotOp>(time); break;
	case 0x3B8: executeLmmc<Graphic7Mode, TImpOp>(time); break;
	case 0x3B9: executeLmmc<Graphic7Mode, TAndOp>(time); break;
	case 0x3BA: executeLmmc<Graphic7Mode, TOrOp >(time); break;
	case 0x3BB: executeLmmc<Graphic7Mode, TXorOp>(time); break;
	case 0x3BC: executeLmmc<Graphic7Mode, TNotOp>(time); break;
	case 0x3B5: case 0x3B6: case 0x3B7: case 0x3BD: case 0x3BE: case 0x3BF:
		executeLmmc<Graphic7Mode, DummyOp>(time); break;
	case 0x4B0: executeLmmc<NonBitmapMode,  ImpOp>(time); break;
	case 0x4B1: executeLmmc<NonBitmapMode,  AndOp>(time); break;
	case 0x4B2: executeLmmc<NonBitmapMode,  OrOp >(time); break;
	case 0x4B3: executeLmmc<NonBitmapMode,  XorOp>(time); break;
	case 0x4B4: executeLmmc<NonBitmapMode,  NotOp>(time); break;
	case 0x4B8: executeLmmc<NonBitmapMode, TImpOp>(time); break;
	case 0x4B9: executeLmmc<NonBitmapMode, TAndOp>(time); break;
	case 0x4BA: executeLmmc<NonBitmapMode, TOrOp >(time); break;
	case 0x4BB: executeLmmc<NonBitmapMode, TXorOp>(time); break;
	case 0x4BC: executeLmmc<NonBitmapMode, TNotOp>(time); break;
	case 0x4B5: case 0x4B6: case 0x4B7: case 0x4BD: case 0x4BE: case 0x4BF:
		executeLmmc<NonBitmapMode, DummyOp>(time); break;

	case 0x0C0: case 0x0C1: case 0x0C2: case 0x0C3:
	case 0x0C4: case 0x0C5: case 0x0C6: case 0x0C7:
	case 0x0C8: case 0x0C9: case 0x0CA: case 0x0CB:
	case 0x0CC: case 0x0CD: case 0x0CE: case 0x0CF:
		executeHmmv<Graphic4Mode>(time); break;
	case 0x1C0: case 0x1C1: case 0x1C2: case 0x1C3:
	case 0x1C4: case 0x1C5: case 0x1C6: case 0x1C7:
	case 0x1C8: case 0x1C9: case 0x1CA: case 0x1CB:
	case 0x1CC: case 0x1CD: case 0x1CE: case 0x1CF:
		executeHmmv<Graphic5Mode>(time); break;
	case 0x2C0: case 0x2C1: case 0x2C2: case 0x2C3:
	case 0x2C4: case 0x2C5: case 0x2C6: case 0x2C7:
	case 0x2C8: case 0x2C9: case 0x2CA: case 0x2CB:
	case 0x2CC: case 0x2CD: case 0x2CE: case 0x2CF:
		executeHmmv<Graphic6Mode>(time); break;
	case 0x3C0: case 0x3C1: case 0x3C2: case 0x3C3:
	case 0x3C4: case 0x3C5: case 0x3C6: case 0x3C7:
	case 0x3C8: case 0x3C9: case 0x3CA: case 0x3CB:
	case 0x3CC: case 0x3CD: case 0x3CE: case 0x3CF:
		executeHmmv<Graphic7Mode>(time); break;
	case 0x4C0: case 0x4C1: case 0x4C2: case 0x4C3:
	case 0x4C4: case 0x4C5: case 0x4C6: case 0x4C7:
	case 0x4C8: case 0x4C9: case 0x4CA: case 0x4CB:
	case 0x4CC: case 0x4CD: case 0x4CE: case 0x4CF:
		executeHmmv<NonBitmapMode>(time); break;

	case 0x0D0: case 0x0D1: case 0x0D2: case 0x0D3:
	case 0x0D4: case 0x0D5: case 0x0D6: case 0x0D7:
	case 0x0D8: case 0x0D9: case 0x0DA: case 0x0DB:
	case 0x0DC: case 0x0DD: case 0x0DE: case 0x0DF:
		executeHmmm<Graphic4Mode>(time); break;
	case 0x1D0: case 0x1D1: case 0x1D2: case 0x1D3:
	case 0x1D4: case 0x1D5: case 0x1D6: case 0x1D7:
	case 0x1D8: case 0x1D9: case 0x1DA: case 0x1DB:
	case 0x1DC: case 0x1DD: case 0x1DE: case 0x1DF:
		executeHmmm<Graphic5Mode>(time); break;
	case 0x2D0: case 0x2D1: case 0x2D2: case 0x2D3:
	case 0x2D4: case 0x2D5: case 0x2D6: case 0x2D7:
	case 0x2D8: case 0x2D9: case 0x2DA: case 0x2DB:
	case 0x2DC: case 0x2DD: case 0x2DE: case 0x2DF:
		executeHmmm<Graphic6Mode>(time); break;
	case 0x3D0: case 0x3D1: case 0x3D2: case 0x3D3:
	case 0x3D4: case 0x3D5: case 0x3D6: case 0x3D7:
	case 0x3D8: case 0x3D9: case 0x3DA: case 0x3DB:
	case 0x3DC: case 0x3DD: case 0x3DE: case 0x3DF:
		executeHmmm<Graphic7Mode>(time); break;
	case 0x4D0: case 0x4D1: case 0x4D2: case 0x4D3:
	case 0x4D4: case 0x4D5: case 0x4D6: case 0x4D7:
	case 0x4D8: case 0x4D9: case 0x4DA: case 0x4DB:
	case 0x4DC: case 0x4DD: case 0x4DE: case 0x4DF:
		executeHmmm<NonBitmapMode>(time); break;

	case 0x0E0: case 0x0E1: case 0x0E2: case 0x0E3:
	case 0x0E4: case 0x0E5: case 0x0E6: case 0x0E7:
	case 0x0E8: case 0x0E9: case 0x0EA: case 0x0EB:
	case 0x0EC: case 0x0ED: case 0x0EE: case 0x0EF:
		executeYmmm<Graphic4Mode>(time); break;
	case 0x1E0: case 0x1E1: case 0x1E2: case 0x1E3:
	case 0x1E4: case 0x1E5: case 0x1E6: case 0x1E7:
	case 0x1E8: case 0x1E9: case 0x1EA: case 0x1EB:
	case 0x1EC: case 0x1ED: case 0x1EE: case 0x1EF:
		executeYmmm<Graphic5Mode>(time); break;
	case 0x2E0: case 0x2E1: case 0x2E2: case 0x2E3:
	case 0x2E4: case 0x2E5: case 0x2E6: case 0x2E7:
	case 0x2E8: case 0x2E9: case 0x2EA: case 0x2EB:
	case 0x2EC: case 0x2ED: case 0x2EE: case 0x2EF:
		executeYmmm<Graphic6Mode>(time); break;
	case 0x3E0: case 0x3E1: case 0x3E2: case 0x3E3:
	case 0x3E4: case 0x3E5: case 0x3E6: case 0x3E7:
	case 0x3E8: case 0x3E9: case 0x3EA: case 0x3EB:
	case 0x3EC: case 0x3ED: case 0x3EE: case 0x3EF:
		executeYmmm<Graphic7Mode>(time); break;
	case 0x4E0: case 0x4E1: case 0x4E2: case 0x4E3:
	case 0x4E4: case 0x4E5: case 0x4E6: case 0x4E7:
	case 0x4E8: case 0x4E9: case 0x4EA: case 0x4EB:
	case 0x4EC: case 0x4ED: case 0x4EE: case 0x4EF:
		executeYmmm<NonBitmapMode>(time); break;

	case 0x0F0: case 0x0F1: case 0x0F2: case 0x0F3:
	case 0x0F4: case 0x0F5: case 0x0F6: case 0x0F7:
	case 0x0F8: case 0x0F9: case 0x0FA: case 0x0FB:
	case 0x0FC: case 0x0FD: case 0x0FE: case 0x0FF:
		executeHmmc<Graphic4Mode>(time); break;
	case 0x1F0: case 0x1F1: case 0x1F2: case 0x1F3:
	case 0x1F4: case 0x1F5: case 0x1F6: case 0x1F7:
	case 0x1F8: case 0x1F9: case 0x1FA: case 0x1FB:
	case 0x1FC: case 0x1FD: case 0x1FE: case 0x1FF:
		executeHmmc<Graphic5Mode>(time); break;
	case 0x2F0: case 0x2F1: case 0x2F2: case 0x2F3:
	case 0x2F4: case 0x2F5: case 0x2F6: case 0x2F7:
	case 0x2F8: case 0x2F9: case 0x2FA: case 0x2FB:
	case 0x2FC: case 0x2FD: case 0x2FE: case 0x2FF:
		executeHmmc<Graphic6Mode>(time); break;
	case 0x3F0: case 0x3F1: case 0x3F2: case 0x3F3:
	case 0x3F4: case 0x3F5: case 0x3F6: case 0x3F7:
	case 0x3F8: case 0x3F9: case 0x3FA: case 0x3FB:
	case 0x3FC: case 0x3FD: case 0x3FE: case 0x3FF:
		executeHmmc<Graphic7Mode>(time); break;
	case 0x4F0: case 0x4F1: case 0x4F2: case 0x4F3:
	case 0x4F4: case 0x4F5: case 0x4F6: case 0x4F7:
	case 0x4F8: case 0x4F9: case 0x4FA: case 0x4FB:
	case 0x4FC: case 0x4FD: case 0x4FE: case 0x4FF:
		executeHmmc<NonBitmapMode>(time); break;

	default:
		UNREACHABLE;
	}
}

void VDPCmdEngine::reportVdpCommand() const
{
	static constexpr std::array<std::string_view, 16> COMMANDS = {
		" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
		" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
	};
	static constexpr std::array<std::string_view, 16> OPS = {
		"IMP ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
		"TIMP","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
	};

	std::cerr << "VDPCmd " << COMMANDS[CMD >> 4] << '-' << OPS[CMD & 15]
		<<  '(' << int(SX) << ',' << int(SY) << ")->("
		        << int(DX) << ',' << int(DY) << ")," << int(COL)
		<< " [" << int((ARG & DIX) ? -int(NX) : int(NX))
		<<  ',' << int((ARG & DIY) ? -int(NY) : int(NY)) << "]\n";
}

void VDPCmdEngine::commandDone(EmuTime::param time)
{
	// Note: TR is not reset yet; it is reset when S#2 is read next.
	status &= 0xFE; // reset CE
	executingProbe = false;
	CMD = 0;
	setStatusChangeTime(EmuTime::infinity());
	vram.cmdReadWindow.disable(time);
	vram.cmdWriteWindow.disable(time);
}


// version 1: initial version
// version 2: replaced member 'Clock<> clock' with 'EmuTime time'
// version 3: added 'phase', 'tmpSrc', 'tmpDst'
// version 4: added 'lastXXX'. This is only used for debugging, so you could ask
//            why this needs to be serialized. Maybe for savestate/replay-files
//            it's not super useful, but it is important for reverse during a
//            debug session.
template<typename Archive>
void VDPCmdEngine::serialize(Archive& ar, unsigned version)
{
	// In older (development) versions CMD was split in a CMD and a LOG
	// member, though it was combined for the savestate. Only the CMD part
	// was guaranteed to be zero when no command was executing. So when
	// loading an older savestate this can still be the case.

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("time", engineTime);
	} else {
		// in version 1, the 'engineTime' member had type 'Clock<>'
		assert(Archive::IS_LOADER);
		VDP::VDPClock clock(EmuTime::dummy());
		ar.serialize("clock", clock);
		engineTime = clock.getTime();
	}
	ar.serialize("statusChangeTime", statusChangeTime,
	             "scrMode",          scrMode,
	             "status",           status,
	             "transfer",         transfer,
	             "SX",  SX,
	             "SY",  SY,
	             "DX",  DX,
	             "DY",  DY,
	             "NX",  NX,
	             "NY",  NY,
	             "ASX", ASX,
	             "ADX", ADX,
	             "ANX", ANX,
	             "COL", COL,
	             "ARG", ARG,
	             "CMD", CMD);

	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("phase",  phase,
		             "tmpSrc", tmpSrc,
		             "tmpDst", tmpDst);
	} else {
		assert(Archive::IS_LOADER);
		phase = tmpSrc = tmpDst = 0;
	}

	if constexpr (Archive::IS_LOADER) {
		if (CMD & 0xF0) {
			assert(scrMode >= 0);
		} else {
			CMD = 0; // see note above
		}
	}

	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("lastSX",  lastSX,
		             "lastSY",  lastSY,
		             "lastDX",  lastDX,
		             "lastDY",  lastDY,
		             "lastNX",  lastNX,
		             "lastNY",  lastNY,
		             "lastCOL", lastCOL,
		             "lastARG", lastARG,
		             "lastCMD", lastCMD);
	} else {
		assert(Archive::IS_LOADER);
		lastSX = SX;
		lastSY = SY;
		lastDX = DX;
		lastDY = DY;
		lastNX = NX;
		lastNY = NY;
		lastCOL = COL;
		lastARG = ARG;
		lastCMD = CMD;
	}
}
INSTANTIATE_SERIALIZE_METHODS(VDPCmdEngine);

} // namespace openmsx
