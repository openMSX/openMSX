#include "V9990CmdEngine.hh"

#include "V9990.hh"
#include "V9990DisplayTiming.hh"
#include "V9990VRAM.hh"

#include "BooleanSetting.hh"
#include "Clock.hh"
#include "EnumSetting.hh"
#include "MSXMotherBoard.hh"
#include "MemBuffer.hh"
#include "RenderSettings.hh"
#include "serialize.hh"

#include "checked_cast.hh"
#include "narrow.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace openmsx {

static constexpr unsigned maxLength = 171; // The maximum value from the xxx_TIMING tables below
static constexpr EmuDuration d_(unsigned x)
{
	assert(x <= maxLength);
	return Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND>::duration(x);
}
using EDStorage = EmuDurationStorageFor<d_(maxLength).toUint64()>;
static constexpr EDStorage d(unsigned x) { return EDStorage{d_(x)}; }
using A  = std::array<const EDStorage, 4>;
using A2 = std::array<const A, 3>;
using TimingTable = std::span<const A2, 4>;

// 1st index  B0/2/4, B1/3/7, P1, P2
// 2nd index  sprites-ON, sprites-OFF, display-OFF
// 3th index  2bpp, 4bpp, 8bpp, 16bpp
//            (for P1/P2 fill in the same value 4 times)
static constexpr std::array SRCH_TIMING = {
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}  // TODO
};

[[nodiscard]] static EmuDuration getTiming(const V9990CmdEngine& cmdEngine, TimingTable table)
{
	if (cmdEngine.getBrokenTiming()) [[unlikely]] return {};

	const auto& vdp = cmdEngine.getVDP();
	auto mode = vdp.getDisplayMode();
	unsigned idx1 = (mode == V9990DisplayMode::P1) ? 2 :
	                (mode == V9990DisplayMode::P2) ? 3 :
	                (vdp.isOverScan()) ? 0 : 1;
	unsigned idx2 = vdp.isDisplayEnabled() ? (vdp.spritesEnabled() ? 0 : 1)
	                                       : 2;
	unsigned idx3 = vdp.getColorDepth();
	return EmuDuration(table[idx1][idx2][idx3]);
}



// Command timing from VRAM accesses.
//
// Measured on a real V9990 in every display mode but with the 25 MHz
// crystal (doc/internal/v9990-cmd-timing). The command engine works on
// 16-bit words: one byte from each of the two VRAM chips. Every word a
// command touches, source or destination, costs two accesses, and some
// 'think' time that depends on the command and on the pixels in a word.
// When an access can happen depends on the raster:
//  - Display off, or outside the picture's lines: any time.
//  - P1 and P2, in the picture's lines:
//     - during the 256 active dots (of P1): 48 slots, evenly spread;
//     - in the horizontal blank, with sprites on: one word (2 accesses) 21
//       clocks after the active part ends; the rest is taken by the sprite
//       fetches. With sprites off: any time for the first 44.5 clocks.
//  - Bitmap modes, in the picture's lines: any time, but for a stretch at
//    the end of the horizontal blank: 21.6 clocks with the cursor on, 3.4
//    with it off (more in the overscan modes).
// In the overscan modes (B0, B2, B4) the engine itself runs from the slower
// clock: all its times are 1.555 times longer.
//
// The time unit here is 1/5 of a UC tick (1/60 of a Z80 clock). Tables are
// indexed by the engine's mode: P1, P2, 2 bpp, 4 bpp, 8 bpp, 16 bpp.
namespace Tm {
enum : unsigned { P1, P2, BPP2, BPP4, BPP8, BPP16 };

static constexpr int SUB = 5;
using Clk = Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND * SUB>;
static constexpr int64_t LINE   = V9990DisplayTiming::UC_TICKS_PER_LINE * SUB;
static constexpr int64_t ACTIVE = 2048 * SUB; // 256 dots of P1
static constexpr int     SLOTS  = 48;         // in the active part
// The first active slot, from the end of openMSX's left border. Measured
// against the line interrupt: the slots start 12 clocks before it.
static constexpr int64_t FIRST_SLOT = -3240;
// P1, P2: horizontal blank, from the end of the active part.
static constexpr int64_t SPRITES_ON_START  = 1281; // 21.35 clocks
static constexpr int64_t SPRITES_ON_LEN    =   60; //  1 clock: one word
static constexpr int64_t SPRITES_OFF_START =    0;
static constexpr int64_t SPRITES_OFF_LEN   = 2670; // 44.5 clocks
// Bitmap modes: the blocked stretch ends 5 clocks before the first
// slot's place. Its length: cursor on, cursor off.
static constexpr int64_t BLOCK_END = 300;
static constexpr std::array<int64_t, 2> BLOCK_LEN          = {1296, 204};
static constexpr std::array<int64_t, 2> BLOCK_LEN_OVERSCAN = {1710, 258};
// The engine's times in the overscan modes: 1.555 times longer.
static constexpr int OVERSCAN_MUL = 311;
static constexpr int OVERSCAN_DIV = 200;

using T6 = std::array<int, 6>;
static constexpr T6 PIXELS_PER_WORD = {2, 4, 8, 4, 2, 1};
// From one access to the engine's next.
static constexpr T6 ACCESS = {42, 41, 41, 41, 41, 41}; // 0.70 clocks
// Extra time at the start of each row. In P1 a row of one pixel takes
// less.
static constexpr T6 ROW_LMMV = {121, 40,  0, 36, 82,  82};
static constexpr T6 ROW_LMMM = {153, 72, 24, 70, 71, 149};
static constexpr int ROW_LMMV_1 =  80;
static constexpr int ROW_LMMM_1 =  74;
static constexpr int ROW_BMLX_P1 = 150;
// Extra time for each word, on top of its two accesses.
static constexpr T6 WORD_LMMV = { 0, 44, 104, 42,  1,  1};
static constexpr T6 WORD_LMMM = {11, 15,  49,  8,  8,  8}; // destination word
static constexpr T6 WORD_CMMM = { 6, 26,  67, 23,  7,  4}; // destination word
// LMMM with source and destination at different places in their words:
// for each pixel.
static constexpr T6 SHIFT_LMMM = {29, 11, 4, 11, 9, 0};
// BMLL with source and destination bytes in different chips: for each
// byte.
static constexpr int CROSS_BMLL = 37;
// Extra time for each linear byte.
static constexpr T6 BYTE_BMXL = {20, 7, 24, 4, 4, 4};
static constexpr T6 BYTE_BMLX = {44, 7, 24, 4, 4, 4};
static constexpr T6 BYTE_BMLL = { 5, 7,  4, 4, 4, 4};
// Extra time for each pixel.
static constexpr T6 PIXEL_LINE = {76, 78, 78, 78, 78, 78};

// The least time that a pixel (BMLL: a byte) can take, in any mode. Used
// for a lower bound on a command's end.
static constexpr int64_t MIN_PIXEL_LMMV = 20;
static constexpr int64_t MIN_PIXEL_COPY = 24; // LMMM, BMXL, BMLX
static constexpr int64_t MIN_BYTE_BMLL  = 70;
static constexpr int64_t MIN_PIXEL_LINE = 120;
// The most of that time that one pending step can cover.
static constexpr int64_t MAX_PENDING = 4 * MIN_PIXEL_LINE;

// Extra time at the start of a row of 'nx' pixels.
[[nodiscard]] static constexpr int rowLMMV(unsigned m, unsigned nx) {
	return ((m == P1) && (nx == 1)) ? ROW_LMMV_1 : ROW_LMMV[m];
}
[[nodiscard]] static constexpr int rowLMMM(unsigned m, unsigned nx) {
	return ((m == P1) && (nx == 1)) ? ROW_LMMM_1 : ROW_LMMM[m];
}
[[nodiscard]] static constexpr int rowBMLX(unsigned m) {
	return (m == P1) ? ROW_BMLX_P1 : ROW_LMMM[m];
}
[[nodiscard]] static constexpr EmuDuration dur(int64_t n) {
	return Clk::duration(n);
}
// Position of active slot 'k', from the first.
[[nodiscard]] static constexpr int64_t slotPos(int64_t k) {
	return (k * ACTIVE + (SLOTS - 1)) / SLOTS;
}
// 'a / b' rounded down, for a positive 'b'.
[[nodiscard]] static constexpr int64_t divFloor(int64_t a, int64_t b) {
	return (a >= 0) ? (a / b) : -((-a + b - 1) / b);
}
} // namespace Tm

// Does the access model time the running command?
bool V9990CmdEngine::accessTimed() const
{
	return !brokenTiming;
}

// An engine time in the current mode: longer in the overscan modes.
int V9990CmdEngine::engineT(int t) const
{
	return vdp.isOverScan() ? (t * Tm::OVERSCAN_MUL / Tm::OVERSCAN_DIV) : t;
}

// The first moment, not before 'request', that the engine gets a VRAM access.
EmuTime V9990CmdEngine::grantAccess(EmuTime request) const
{
	if (!vdp.isDisplayEnabledThisFrame()) return request;
	// (a frame ends outside the picture)
	if (request <= vdp.getFrameStartTime()) return request;

	Tm::Clk clk(vdp.getFrameStartTime());
	auto t = narrow<int64_t>(clk.getTicksTillUp(request));

	const auto& ver = vdp.getVerticalTiming();
	// The accesses are already rationed in the line before the picture's
	// first (measured in P1: 213 lines, not 212).
	int64_t top    = vdp.getTopBorder() - 1;
	int64_t bottom = top + 1 + ver.display;
	int64_t base   = vdp.getLeftBorder() * Tm::SUB + Tm::FIRST_SLOT;
	bool sprites = vdp.spritesEnabled();
	bool patternMode = timingMode() <= Tm::P2;

	// Horizontal blank: P1, P2: the window [w0, w1) is free; bitmap modes:
	// it is blocked.
	int64_t w0 = 0;
	int64_t w1 = 0;
	if (patternMode) {
		w0 = Tm::ACTIVE + (sprites ? Tm::SPRITES_ON_START
		                           : Tm::SPRITES_OFF_START);
		w1 = w0 + (sprites ? Tm::SPRITES_ON_LEN : Tm::SPRITES_OFF_LEN);
	} else {
		const auto& len = vdp.isOverScan() ? Tm::BLOCK_LEN_OVERSCAN
		                                   : Tm::BLOCK_LEN;
		w1 = Tm::LINE - Tm::BLOCK_END;
		w0 = w1 - len[sprites ? 0 : 1];
	}

	// 'u' counts from the first active slot of the frame's line 0.
	int64_t u = t - base;
	while (true) {
		int64_t line = Tm::divFloor(u, Tm::LINE);
		int64_t x = u - line * Tm::LINE;
		int64_t frameLine =
			line - Tm::divFloor(line, ver.cycle) * ver.cycle;
		if ((frameLine < top) || (frameLine >= bottom)) break; // any time
		if (!patternMode) {
			if ((w0 <= x) && (x < w1)) u = line * Tm::LINE + w1;
			break;
		}
		if (x <= Tm::slotPos(Tm::SLOTS - 1)) {
			int64_t k = x * Tm::SLOTS / Tm::ACTIVE;
			while (Tm::slotPos(k) < x) ++k;
			u = line * Tm::LINE + Tm::slotPos(k);
			break;
		}
		if (x < w0) {
			u = line * Tm::LINE + w0;
			break;
		}
		if (x < w1) break;
		u = (line + 1) * Tm::LINE; // the next line's first slot
	}
	return clk + narrow<uint64_t>(u + base);
}

// One step of a command: 'think' time, then 'accesses' VRAM accesses. Returns
// false if that isn't all done by 'limit'; 'nextDone' is then when it will be.
bool V9990CmdEngine::accessStep(EmuTime limit, unsigned accesses, int think)
{
	auto access = Tm::dur(engineT(Tm::ACCESS[timingMode()]));
	EmuTime t = engineTime + Tm::dur(engineT(think));
	repeat(accesses, [&] {
		t = grantAccess(t) + access;
	});
	if (t > limit) {
		nextDone = t;
		return false;
	}
	engineTime = t;
	return true;
}

// One step of a command's loop.
bool V9990CmdEngine::step(EmuTime limit, bool timed,
                          unsigned accesses, int think)
{
	if (timed) return accessStep(limit, accesses, think);
	// instantaneous (broken) timing: everything at the first later sync
	return engineTime < limit;
}

// Does the pixel at 'x' start a new VRAM word, going in direction 'dx'? 'n'
// is the pixels in a word (in P1: the two pixels that share a byte in each
// chip).
[[nodiscard]] static constexpr bool wordStart(
	uint16_t x, uint16_t dx, unsigned n)
{
	return (x & (n - 1)) == ((dx == 1) ? 0 : (n - 1));
}

// The words that the next 'count' pixels of a rectangle command start, from
// column 'x' with 'anx' pixels left in the row and 'any' rows left.
unsigned V9990CmdEngine::wordStarts(
	uint16_t x, uint16_t dx, unsigned n, unsigned count) const
{
	unsigned result = 0;
	uint16_t anx = ANX;
	uint16_t any = ANY;
	repeat(count, [&] {
		if (any == 0) return;
		if ((anx == getWrappedNX()) || wordStart(x, dx, n)) ++result;
		x += dx;
		if (!--anx) {
			x -= uint16_t(NX * dx);
			--any;
			anx = getWrappedNX();
		}
	});
	return result;
}

// Lazily initialized LUT to speed up logical operations:
//  - 1st index is the mode: 2,4,8 bpp or 'not-transparent'
//  - 2nd index is the logical operation: one of the 16 possible binary functions
// * Each entry contains a 256x256 byte array, that array is indexed using
//   destination and source byte (in that order).
// * A fully populated logOpLUT would take 4MB, however the vast majority of
//   this table is (almost) never used. So we save quite some memory (and
//   startup time) by lazily initializing this table.
enum class Log : uint8_t {
	NO_T, BPP2, BPP4, BPP8,
	NUM
};
static array_with_enum_index<Log, std::array<MemBuffer<uint8_t>, 16>> logOpLUT;

// to speedup calculating logOpLUT
static constexpr auto bitLUT = [] {
	std::array<std::array<std::array<std::array<uint8_t, 2>, 2>, 16>, 8> result = {};
	for (auto op : xrange(16)) {
		unsigned tmp = op;
		for (auto src : xrange(2)) {
			for (auto dst : xrange(2)) {
				unsigned b = tmp & 1;
				for (auto bit : xrange(8)) {
					result[bit][op][src][dst] = narrow<uint8_t>(b << bit);
				}
				tmp >>= 1;
			}
		}
	}
	return result;
}();

[[nodiscard]] static constexpr uint8_t func01(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x03) == 0) return dst & 0x03;
	uint8_t res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	return res;
}
[[nodiscard]] static constexpr uint8_t func23(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0C) == 0) return dst & 0x0C;
	uint8_t res = 0;
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
[[nodiscard]] static constexpr uint8_t func45(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x30) == 0) return dst & 0x30;
	uint8_t res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	return res;
}
[[nodiscard]] static constexpr uint8_t func67(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xC0) == 0) return dst & 0xC0;
	uint8_t res = 0;
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

[[nodiscard]] static constexpr uint8_t func03(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0F) == 0) return dst & 0x0F;
	uint8_t res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
[[nodiscard]] static constexpr uint8_t func47(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xF0) == 0) return dst & 0xF0;
	uint8_t res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

[[nodiscard]] static constexpr uint8_t func07(unsigned op, unsigned src, unsigned dst)
{
	// if (src == 0) return dst;  // handled in fillTable8
	uint8_t res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

static constexpr void fillTableNoT(unsigned op, std::span<uint8_t, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

static constexpr void fillTable2(unsigned op, std::span<uint8_t, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			uint8_t res = 0;
			res |= func01(op, src, dst);
			res |= func23(op, src, dst);
			res |= func45(op, src, dst);
			res |= func67(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static constexpr void fillTable4(unsigned op, std::span<uint8_t, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			uint8_t res = 0;
			res |= func03(op, src, dst);
			res |= func47(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static constexpr void fillTable8(unsigned op, std::span<uint8_t, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		{ // src == 0
			table[dst * 256 + 0  ] = narrow_cast<uint8_t>(dst);
		}
		for (auto src : xrange(1, 256)) { // src != 0
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

[[nodiscard]] static std::span<const uint8_t, 256 * 256> getLogOpImpl(Log mode, unsigned op)
{
	op &= 0x0f;
	auto& lut = logOpLUT[mode][op];
	if (!lut.data()) {
		lut.resize(256 * 256);
		std::span<uint8_t, 256 * 256> s{lut};
		switch (mode) {
		using enum Log;
		case NO_T:
			fillTableNoT(op, s);
			break;
		case BPP2:
			fillTable2(op, s);
			break;
		case BPP4:
			fillTable4(op, s);
			break;
		case BPP8:
			fillTable8(op, s);
			break;
		default:
			UNREACHABLE;
		}
	}
	return std::span<uint8_t, 256 * 256>{lut};
}


static constexpr uint8_t DIY = 0x08;
static constexpr uint8_t DIX = 0x04;
static constexpr uint8_t NEQ = 0x02;
static constexpr uint8_t MAJ = 0x01;

// P1 --------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990P1::getPitch(unsigned width)
{
	return width / 2;
}

inline unsigned V9990CmdEngine::V9990P1::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	//return V9990VRAM::transformP1(((x / 2) & (pitch - 1)) + y * pitch) & 0x7FFFF;
	// TODO figure out exactly how the coordinate system maps to vram in P1
	unsigned addr = V9990VRAM::transformP1(((x / 2) & (pitch - 1)) + y * pitch);
	return (addr & 0x3FFFF) | ((x & 0x200) << 9);
}

inline uint8_t V9990CmdEngine::V9990P1::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline uint8_t V9990CmdEngine::V9990P1::shift(
	uint8_t value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? uint8_t(value >> shift) : uint8_t(value << -shift);
}

inline uint8_t V9990CmdEngine::V9990P1::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990P1::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline uint8_t V9990CmdEngine::V9990P1::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst)
{
	return lut[256 * dst + src];
}

// A P1 write goes out on the 16-bit data bus of both VRAM chips (VRAM0 = layer
// A, VRAM1 = layer B) at the same address. The engine only loads the byte
// lane of the target chip into its data output register; the other lane
// keeps whatever it held from an earlier command write to that chip. Both
// chips are written through WM (R#46 for VRAM0, R#47 for VRAM1) and the
// nibble mask of the target pixel, so with WM = FFFFh the other layer gets the
// stale lane byte at the same address. This is why the manual says that in
// P1 the program must clear the WM byte of the side that isn't the transfer
// destination. Verified against a real V9990 (Tecnobytes PowerGraph) with
// the P1CMD/P1CMD2 probes: LMMV puts its colour on the target lane (FC bits
// 7-0 for either layer, see executeLMMV); LMMM puts the source pixel shifted
// into its nibble, other nibble 0.
// A write that WM masks out completely on the target layer doesn't reach the
// other layer either (measured with VDPEND: with WM = 00FFh, an LMMV into
// layer B leaves layer A alone, whatever the other lane holds).
// CPU writes through port P#0 do not touch this register; reset clears it.
static inline void p1Write(
	V9990VRAM& vram, unsigned addr, uint8_t nibbleMask, uint8_t newColor,
	uint16_t mask, uint16_t& dataLatch)
{
	unsigned lane = (addr & 0x40000) ? 8 : 0;
	dataLatch = uint16_t((dataLatch & ~(0xFF << lane)) | (newColor << lane));
	if (((mask >> lane) & nibbleMask) == 0) return;
	for (unsigned l : {0u, 8u}) {
		auto m = narrow_cast<uint8_t>((mask >> l) & nibbleMask);
		if (m == 0) continue;
		unsigned a = (addr & 0x3FFFF) | (l ? 0x40000 : 0);
		auto d = narrow_cast<uint8_t>(dataLatch >> l);
		auto old = vram.readVRAMDirect(a);
		vram.writeVRAMDirect(a, uint8_t((old & ~m) | (d & m)));
	}
}

inline void V9990CmdEngine::V9990P1::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& dataLatch)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor & shiftMask(x), dstColor);
	p1Write(vram, addr, shiftMask(x), newColor, mask, dataLatch);
}
inline void V9990CmdEngine::V9990P1::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& dataLatch)
{
	auto addr = addressOf(x, y, pitch);
	auto srcColor = narrow_cast<uint8_t>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	p1Write(vram, addr, shiftMask(x), newColor, mask, dataLatch);
}

// P2 --------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990P2::getPitch(unsigned width)
{
	return width / 2;
}

inline unsigned V9990CmdEngine::V9990P2::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	// TODO check
	return V9990VRAM::transformP2(((x / 2) & (pitch - 1)) + y * pitch) & 0x7FFFF;
}

inline uint8_t V9990CmdEngine::V9990P2::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline uint8_t V9990CmdEngine::V9990P2::shift(
	uint8_t value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? uint8_t(value >> shift) : uint8_t(value << -shift);
}

inline uint8_t V9990CmdEngine::V9990P2::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990P2::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline uint8_t V9990CmdEngine::V9990P2::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990P2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & shiftMask(x);
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990P2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto srcColor = narrow_cast<uint8_t>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & (0xF0 >> (4 * (x & 1)));
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

// 2 bpp --------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990Bpp2::getPitch(unsigned width)
{
	return width / 4;
}

inline unsigned V9990CmdEngine::V9990Bpp2::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	return V9990VRAM::transformBx(((x / 4) & (pitch - 1)) + y * pitch) & 0x7FFFF;
}

inline uint8_t V9990CmdEngine::V9990Bpp2::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline uint8_t V9990CmdEngine::V9990Bpp2::shift(
	uint8_t value, unsigned fromX, unsigned toX)
{
	int shift = 2 * (narrow<int>(toX & 3) - narrow<int>(fromX & 3));
	return (shift > 0) ? uint8_t(value >> shift) : uint8_t(value << -shift);
}

inline uint8_t V9990CmdEngine::V9990Bpp2::shiftMask(unsigned x)
{
	return 0xC0 >> (2 * (x & 3));
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990Bpp2::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP2 : Log::NO_T, op);
}

inline uint8_t V9990CmdEngine::V9990Bpp2::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & shiftMask(x);
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto srcColor = narrow_cast<uint8_t>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & (0xC0 >> (2 * (x & 3)));
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

// 4 bpp --------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990Bpp4::getPitch(unsigned width)
{
	return width / 2;
}

inline unsigned V9990CmdEngine::V9990Bpp4::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	return V9990VRAM::transformBx(((x / 2) & (pitch - 1)) + y * pitch) & 0x7FFFF;
}

inline uint8_t V9990CmdEngine::V9990Bpp4::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline uint8_t V9990CmdEngine::V9990Bpp4::shift(
	uint8_t value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? uint8_t(value >> shift) : uint8_t(value << -shift);
}

inline uint8_t V9990CmdEngine::V9990Bpp4::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990Bpp4::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline uint8_t V9990CmdEngine::V9990Bpp4::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp4::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & shiftMask(x);
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp4::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto srcColor = narrow_cast<uint8_t>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t mask2 = mask1 & (0xF0 >> (4 * (x & 1)));
	uint8_t result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

// 8 bpp --------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990Bpp8::getPitch(unsigned width)
{
	return width;
}

inline unsigned V9990CmdEngine::V9990Bpp8::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	return V9990VRAM::transformBx((x & (pitch - 1)) + y * pitch) & 0x7FFFF;
}

inline uint8_t V9990CmdEngine::V9990Bpp8::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline uint8_t V9990CmdEngine::V9990Bpp8::shift(
	uint8_t value, unsigned /*fromX*/, unsigned /*toX*/)
{
	return value;
}

inline uint8_t V9990CmdEngine::V9990Bpp8::shiftMask(unsigned /*x*/)
{
	return 0xFF;
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990Bpp8::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP8 : Log::NO_T, op);
}

inline uint8_t V9990CmdEngine::V9990Bpp8::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint8_t src, uint8_t dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp8::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint8_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t result = (dstColor & ~mask1) | (newColor & mask1);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp8::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t color, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t /*op*/, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto srcColor = narrow_cast<uint8_t>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	auto dstColor = vram.readVRAMDirect(addr);
	auto newColor = logOp(lut, srcColor, dstColor);
	auto mask1 = narrow_cast<uint8_t>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	uint8_t result = (dstColor & ~mask1) | (newColor & mask1);
	vram.writeVRAMDirect(addr, result);
}

// 16 bpp -------------------------------------------------------------
inline unsigned V9990CmdEngine::V9990Bpp16::getPitch(unsigned width)
{
	//return width * 2;
	return width;
}

inline unsigned V9990CmdEngine::V9990Bpp16::addressOf(
	unsigned x, unsigned y, unsigned pitch)
{
	//return V9990VRAM::transformBx(((x * 2) & (pitch - 1)) + y * pitch) & 0x7FFFF;
	return ((x & (pitch - 1)) + y * pitch) & 0x3FFFF;
}

inline uint16_t V9990CmdEngine::V9990Bpp16::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	unsigned addr = addressOf(x, y, pitch);
	return uint16_t(vram.readVRAMDirect(addr + 0x00000) +
	                vram.readVRAMDirect(addr + 0x40000) * 256);
}

inline uint16_t V9990CmdEngine::V9990Bpp16::shift(
	uint16_t value, unsigned /*fromX*/, unsigned /*toX*/)
{
	return value;
}

inline uint16_t V9990CmdEngine::V9990Bpp16::shiftMask(unsigned /*x*/)
{
	return 0xFFFF;
}

inline std::span<const uint8_t, 256 * 256> V9990CmdEngine::V9990Bpp16::getLogOpLUT(uint8_t op)
{
	return getLogOpImpl(Log::NO_T, op);
}

inline uint16_t V9990CmdEngine::V9990Bpp16::logOp(
	std::span<const uint8_t, 256 * 256> lut, uint16_t src, uint16_t dst, bool transp)
{
	if (transp && (src == 0)) return dst;
	return uint16_t((lut[((dst & 0x00FF) << 8) + ((src & 0x00FF) >> 0)] << 0) +
	                (lut[((dst & 0xFF00) << 0) + ((src & 0xFF00) >> 8)] << 8));
}

inline void V9990CmdEngine::V9990Bpp16::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = uint16_t(vram.readVRAMDirect(addr + 0x00000) +
	                         vram.readVRAMDirect(addr + 0x40000) * 256);
	auto newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	uint16_t result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, narrow_cast<uint8_t>(result & 0xFF));
	vram.writeVRAMDirect(addr + 0x40000, narrow_cast<uint8_t>(result >> 8));
}

inline void V9990CmdEngine::V9990Bpp16::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	uint16_t srcColor, uint16_t mask, std::span<const uint8_t, 256 * 256> lut, uint8_t op, uint16_t& /*dataLatch*/)
{
	auto addr = addressOf(x, y, pitch);
	auto dstColor = uint16_t(vram.readVRAMDirect(addr + 0x00000) +
	                         vram.readVRAMDirect(addr + 0x40000) * 256);
	auto newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	uint16_t result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, narrow_cast<uint8_t>(result & 0xFF));
	vram.writeVRAMDirect(addr + 0x40000, narrow_cast<uint8_t>(result >> 8));
}

// ====================================================================
/** Constructor
  */
V9990CmdEngine::V9990CmdEngine(V9990& vdp_, EmuTime time_,
                               RenderSettings& settings_)
	: settings(settings_), vdp(vdp_), vram(vdp.getVRAM()), engineTime(time_)
{
	cmdTraceSetting = vdp.getMotherBoard().getSharedStuff<BooleanSetting>(
		"v9990cmdtrace",
		vdp.getCommandController(), "v9990cmdtrace",
		"V9990 command tracing on/off", false);

	auto& cmdTimingSetting = settings.getCmdTimingSetting();
	update(cmdTimingSetting);
	cmdTimingSetting.attach(*this);

	reset(time_);

	// avoid UMR on savestate
	srcAddress = dstAddress = nbBytes = 0;
	ASX = ADX = ANX = ANY = 0;
	SX = SY = DX = DY = NX = NY = 0;
	WM = fgCol = bgCol = 0;
	ARG = LOG = 0;
	data = bitsLeft = partial = 0;
}

V9990CmdEngine::~V9990CmdEngine()
{
	settings.getCmdTimingSetting().detach(*this);
}

void V9990CmdEngine::reset(EmuTime /*time*/)
{
	CMD = 0;
	status = 0;
	dataLatch = 0;
	endAfterRead = false;
}

void V9990CmdEngine::setCmdReg(uint8_t reg, uint8_t value, EmuTime time)
{
	sync(time);
	switch(reg - 32) {
	case  0: // SX low
		SX = uint16_t((SX & 0x0700) | ((value & 0xFF) << 0));
		break;
	case  1: // SX high
		SX = uint16_t((SX & 0x00FF) | ((value & 0x07) << 8));
		break;
	case  2: // SY low
		SY = uint16_t((SY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  3: // SY high
		SY = uint16_t((SY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case  4: // DX low
		DX = uint16_t((DX & 0x0700) | ((value & 0xFF) << 0));
		break;
	case  5: // DX high
		DX = uint16_t((DX & 0x00FF) | ((value & 0x07) << 8));
		break;
	case  6: // DY low
		DY = uint16_t((DY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  7: // DY high
		DY = uint16_t((DY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case  8: // NX low
		NX = uint16_t((NX & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  9: // NX high
		NX = uint16_t((NX & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case 10: // NY low
		NY = uint16_t((NY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case 11: // NY high
		NY = uint16_t((NY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case 12: // ARG
		ARG = value & 0x0F;
		break;
	case 13: // LOGOP
		LOG = value & 0x1F;
		break;
	case 14: // write mask low
		WM = uint16_t((WM & 0xFF00) | (value << 0));
		break;
	case 15: // write mask high
		WM = uint16_t((WM & 0x00FF) | (value << 8));
		break;
	// Measured: once a SRCH sets BD, it stays set through other commands
	// until FC is written. (The probe always wrote both bytes, so whether
	// one byte alone clears it is not known.)
	case 16: // Font color - FG low
		fgCol = uint16_t((fgCol & 0xFF00) | (value << 0));
		status &= ~BD;
		break;
	case 17: // Font color - FG high
		fgCol = uint16_t((fgCol & 0x00FF) | (value << 8));
		status &= ~BD;
		break;
	case 18: // Font color - BG low
		bgCol = uint16_t((bgCol & 0xFF00) | (value << 0));
		break;
	case 19: // Font color - BG high
		bgCol = uint16_t((bgCol & 0x00FF) | (value << 8));
		break;
	case 20: { // CMD
		CMD = value;
		if (cmdTraceSetting->getBoolean()) {
			reportV9990Command();
		}
		status |= CE;

		// TODO do this when mode changes instead of at the start of a command.
		setCommandMode();

		//currentCommand->start(time);
		switch (cmdMode | (CMD >> 4)) {
			case 0x00: case 0x10: case 0x20: case 0x30: case 0x40: case 0x50:
				startSTOP(time); break;

			case 0x01: case 0x11: case 0x21: case 0x31: case 0x41:
				startLMMC  (time); break;
			case 0x51:
				startLMMC16(time); break;

			case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
				startLMMV(time); break;

			case 0x03: case 0x13: case 0x23: case 0x33: case 0x43:
				startLMCM  (time); break;
			case 0x53:
				startLMCM16(time); break;

			case 0x04: case 0x14: case 0x24: case 0x34: case 0x44: case 0x54:
				startLMMM(time); break;

			case 0x05: case 0x15: case 0x25: case 0x35: case 0x45: case 0x55:
				startCMMC(time); break;

			case 0x06: case 0x16: case 0x26: case 0x36: case 0x46: case 0x56:
				startCMMK(time); break;

			case 0x07: case 0x17: case 0x27: case 0x37: case 0x47: case 0x57:
				startCMMM(time); break;

			case 0x08: case 0x18: case 0x28: case 0x38: case 0x48: case 0x58:
				startBMXL(time); break;

			case 0x09: case 0x19: case 0x29: case 0x39: case 0x49: case 0x59:
				startBMLX(time); break;

			case 0x0A: case 0x1A: case 0x2A: case 0x3A: case 0x4A:
				startBMLL  (time); break;
			case 0x5A:
				startBMLL16(time); break;

			case 0x0B: case 0x1B: case 0x2B: case 0x3B: case 0x4B: case 0x5B:
				startLINE(time); break;

			case 0x0C: case 0x1C: case 0x2C: case 0x3C: case 0x4C: case 0x5C:
				startSRCH(time); break;

			case 0x0D: startPOINT<V9990P1   >(time); break;
			case 0x1D: startPOINT<V9990P2   >(time); break;
			case 0x2D: startPOINT<V9990Bpp2 >(time); break;
			case 0x3D: startPOINT<V9990Bpp4 >(time); break;
			case 0x4D: startPOINT<V9990Bpp8 >(time); break;
			case 0x5D: startPOINT<V9990Bpp16>(time); break;

			case 0x0E: startPSET<V9990P1   >(time); break;
			case 0x1E: startPSET<V9990P2   >(time); break;
			case 0x2E: startPSET<V9990Bpp2 >(time); break;
			case 0x3E: startPSET<V9990Bpp4 >(time); break;
			case 0x4E: startPSET<V9990Bpp8 >(time); break;
			case 0x5E: startPSET<V9990Bpp16>(time); break;

			case 0x0F: case 0x1F: case 0x2F: case 0x3F: case 0x4F: case 0x5F:
				startADVN(time); break;

			default: UNREACHABLE;
		}

		if (accessTimed()) {
			nextDone = engineTime;
		}

		// Finish command now if instantaneous command timing is active.
		if (brokenTiming) {
			sync(time);
		}
		break;
	}
	}
}

void V9990CmdEngine::setCommandMode()
{
	auto dispMode = vdp.getDisplayMode();
	if (dispMode == V9990DisplayMode::P1) {
		cmdMode = 0 << 4; // P1;
	} else if (dispMode == V9990DisplayMode::P2) {
		cmdMode = 1 << 4; // P2;
	} else { // Bx
		switch (vdp.getColorMode()) {
			using enum V9990ColorMode;
			default:
				UNREACHABLE; break;
			case BP2:
				cmdMode = 2 << 4; // BPP2;
				break;
			case PP:
			case BP4:
				cmdMode = 3 << 4; // BPP4;
				break;
			case BYUV:
			case BYUVP:
			case BYJK:
			case BYJKP:
			case BD8:
			case BP6:
				cmdMode = 4 << 4; // BPP8;
				break;
			case BD16:
				cmdMode = 5 << 4; // BPP16;
				break;
		}
	}
}

void V9990CmdEngine::reportV9990Command() const
{
	static constexpr std::array<std::string_view, 16> COMMANDS = {
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
	          << " ARG=" << std::hex << int(ARG)
	          << " LOG=" << std::hex << int(LOG)
	          << " WM="  << std::hex << WM
	          << " FC="  << std::hex << fgCol
	          << " BC="  << std::hex << bgCol
	          << " CMD=" << std::hex << int(CMD)
	          << '\n';
}

void V9990CmdEngine::update(const Setting& setting) noexcept
{
	brokenTiming = checked_cast<const EnumSetting<bool>&>(setting).getEnum();
}

// STOP
void V9990CmdEngine::startSTOP(EmuTime time)
{
	endDecNY();
	cmdReady(time);
}

void V9990CmdEngine::executeSTOP(EmuTime /*limit*/)
{
	UNREACHABLE;
}

// LMMC
void V9990CmdEngine::startLMMC(EmuTime /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status |= TR;
}
void V9990CmdEngine::startLMMC16(EmuTime time)
{
	bitsLeft = 1;
	startLMMC(time);
}

template<>
void V9990CmdEngine::executeLMMC<V9990CmdEngine::V9990Bpp16>(EmuTime limit)
{
	if (!(status & TR)) {
		status |= TR;
		if (bitsLeft) {
			bitsLeft = 0;
			partial = data;
		} else {
			bitsLeft = 1;
			auto value = uint16_t((data << 8) | partial);
			unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
			auto lut = V9990Bpp16::getLogOpLUT(LOG);
			V9990Bpp16::pset(vram, DX, DY, pitch, value, WM, lut, LOG, dataLatch);
			uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
			DX += dx;
			if (!--ANX) {
				uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
				DX -= uint16_t(NX * dx);
				DY += dy;
				if (!--ANY) {
					SX = 0; // SX is the X counter
					endRect();
					cmdReady(limit);
				} else {
					ANX = getWrappedNX();
				}
			}
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeLMMC(EmuTime limit)
{
	if (!(status & TR)) {
		status |= TR;
		unsigned pitch = Mode::getPitch(vdp.getImageWidth());
		auto lut = Mode::getLogOpLUT(LOG);
		for (int i = 0; (ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			uint8_t d = Mode::shift(data, i, DX);
			Mode::pset(vram, DX, DY, pitch, d, WM, lut, LOG, dataLatch);

			uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
			DX += dx;
			if (!--ANX) {
				uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
				DX -= uint16_t(NX * dx);
				DY += dy;
				if (!--ANY) {
					SX = 0; // SX is the X counter
					endRect();
					cmdReady(limit);
				} else {
					ANX = getWrappedNX();
				}
			}
		}
	}
}

// LMMV
void V9990CmdEngine::startLMMV(EmuTime time)
{
	engineTime = time;
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<typename Mode>
void V9990CmdEngine::executeLMMV(EmuTime limit)
{
	// TODO can be optimized a lot

	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	// In P1, LMMV draws with FC bits 7-0 into either layer (measured on a
	// real V9990: an LMMV into layer B ignores FC bits 15-8).
	uint16_t color = std::is_same_v<Mode, V9990P1>
	               ? uint16_t((fgCol & 0xFF) * 0x0101) : fgCol;
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	const unsigned npw = Tm::PIXELS_PER_WORD[m];
	while (true) {
		bool rowStart = ANX == getWrappedNX();
		bool word = rowStart || wordStart(DX, dx, npw);
		int think = (word ? Tm::WORD_LMMV[m] : 0) +
		            (rowStart ? Tm::rowLMMV(m, NX) : 0);
		if (!step(limit, timed, word ? 2 : 0, think)) break;
		Mode::psetColor(vram, DX, DY, pitch, color, WM, lut, LOG, dataLatch);

		DX += dx;
		if (!--ANX) {
			DX -= uint16_t(NX * dx);
			DY += dy;
			SY += dy; // LMMV also steps SY, like LMMM
			if (!--ANY) {
				SX = 0; // SX is the X counter
				endRect();
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// LMCM
void V9990CmdEngine::startLMCM(EmuTime /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status &= ~TR;
	endAfterRead = false;
}
void V9990CmdEngine::startLMCM16(EmuTime time)
{
	bitsLeft = 0;
	startLMCM(time);
}

template<typename Mode>
void V9990CmdEngine::executeLMCM(EmuTime /*limit*/)
{
	if (!(status & TR)) {
		status |= TR;
		if ((Mode::BITS_PER_PIXEL == 16) && bitsLeft) {
			bitsLeft = 0;
			data = partial;
			return;
		}
		unsigned pitch = Mode::getPitch(vdp.getImageWidth());
		using Type = typename Mode::Type;
		Type d = 0;
		for (int i = 0; (ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			auto src = Mode::point(vram, SX, SY, pitch);
			if constexpr (std::is_same_v<Mode, V9990P1>) {
				// The first pixel loads the whole source byte, its
				// nibbles swapped when needed; a second pixel
				// replaces the other nibble. So when the transfer
				// ends on the first pixel of a byte, the other
				// nibble holds the source's other pixel (measured:
				// F1h, not F0h).
				auto rot = uint8_t((SX ^ i) & 1 ? (src << 4) | (src >> 4) : src);
				d = (i == 0) ? rot : Type((d & ~Mode::shiftMask(i)) | (rot & Mode::shiftMask(i)));
			} else {
				d |= Type(Mode::shift(src, SX, i) & Mode::shiftMask(i));
			}

			uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
			SX += dx;
			if (!--ANX) {
				uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
				SX -= uint16_t(NX * dx);
				SY += dy;
				if (!--ANY) {
					DX = 0; // DX is the X counter
					endRect();
					endAfterRead = true;
				} else {
					ANX = getWrappedNX();
				}
			}
		}
		if constexpr (Mode::BITS_PER_PIXEL == 16) {
			unsigned tmp = d; // workaround for VC++ warning C4333
			                  // (in case Mode::Type == uint8_t and
			                  //          Mode::BITS_PER_PIXEL == 8)
			data = narrow_cast<uint8_t>(tmp & 0xff);
			partial = narrow_cast<uint8_t>(tmp >> 8);
			bitsLeft = 1;
		} else {
			data = uint8_t(d);
		}
	}
}

// LMMM
void V9990CmdEngine::startLMMM(EmuTime time)
{
	engineTime = time;
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<typename Mode>
void V9990CmdEngine::executeLMMM(EmuTime limit)
{
	// TODO can be optimized a lot

	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	const unsigned npw = Tm::PIXELS_PER_WORD[m];
	// the pixel moves to another place in its word
	const int shift = ((SX ^ DX) & (npw - 1)) ? Tm::SHIFT_LMMM[m] : 0;
	while (true) {
		bool rowStart = ANX == getWrappedNX();
		bool dstWord = rowStart || wordStart(DX, dx, npw);
		bool srcWord = rowStart || wordStart(SX, dx, npw);
		unsigned accesses = (dstWord ? 2 : 0) + (srcWord ? 2 : 0);
		int think = (rowStart ? Tm::rowLMMM(m, NX) : 0) +
		            (dstWord ? Tm::WORD_LMMM[m] : 0) + shift;
		if (!step(limit, timed, accesses, think)) break;
		auto src = Mode::point(vram, SX, SY, pitch);
		src = Mode::shift(src, SX, DX);
		Mode::pset(vram, DX, DY, pitch, src, WM, lut, LOG, dataLatch);

		DX += dx;
		SX += dx;
		if (!--ANX) {
			DX -= uint16_t(NX * dx);
			SX -= uint16_t(NX * dx);
			DY += dy;
			SY += dy;
			if (!--ANY) {
				endRect();
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// CMMC
void V9990CmdEngine::startCMMC(EmuTime /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status |= TR;
}

template<typename Mode>
void V9990CmdEngine::executeCMMC(EmuTime limit)
{
	if (!(status & TR)) {
		status |= TR;

		unsigned pitch = Mode::getPitch(vdp.getImageWidth());
		uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
		uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
		auto lut = Mode::getLogOpLUT(LOG);
		for (auto i : xrange(8)) {
			(void)i;
			bool bit = (data & 0x80) != 0;
			data <<= 1;

			uint16_t src = bit ? fgCol : bgCol;
			Mode::psetColor(vram, DX, DY, pitch, src, WM, lut, LOG, dataLatch);

			DX += dx;
			if (!--ANX) {
				DX -= uint16_t(NX * dx);
				DY += dy;
				if (!--ANY) {
					SX = 0; // SX is the X counter
					endRect();
					cmdReady(limit);
					return;
				} else {
					ANX = getWrappedNX();
				}
			}
		}
	}
}

// CMMK
void V9990CmdEngine::startCMMK(EmuTime time)
{
	std::cout << "V9990: CMMK not yet implemented\n";
	// TODO dummy implementation. A V9990 without a kanji ROM (measured)
	// ends at once, but still steps DY by NY and ends with NY = FFFh.
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	DY = uint16_t((DY + getWrappedNY() * dy) & 0xFFF);
	endRect();
	cmdReady(time);
}

void V9990CmdEngine::executeCMMK(EmuTime /*limit*/)
{
	UNREACHABLE;
}

// CMMM
void V9990CmdEngine::startCMMM(EmuTime time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	bitsLeft = 0;
}

template<typename Mode>
void V9990CmdEngine::executeCMMM(EmuTime limit)
{
	// TODO can be optimized a lot

	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	const unsigned npw = Tm::PIXELS_PER_WORD[m];
	while (true) {
		// (a font word is 16 pixels: one access for each font byte)
		bool dstWord = (ANX == getWrappedNX()) || wordStart(DX, dx, npw);
		unsigned accesses = (dstWord ? 2 : 0) + (bitsLeft ? 0 : 1);
		int think = dstWord ? Tm::WORD_CMMM[m] : 0;
		if (!step(limit, timed, accesses, think)) break;
		if (!bitsLeft) {
			data = vram.readVRAMBx(srcAddress++);
			bitsLeft = 8;
		}
		--bitsLeft;
		bool bit = (data & 0x80) != 0;
		data <<= 1;

		uint16_t color = bit ? fgCol : bgCol;
		Mode::psetColor(vram, DX, DY, pitch, color, WM, lut, LOG, dataLatch);

		DX += dx;
		if (!--ANX) {
			DX -= uint16_t(NX * dx);
			DY += dy;
			if (!--ANY) {
				endRect();
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// BMXL
void V9990CmdEngine::startBMXL(EmuTime time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<>
void V9990CmdEngine::executeBMXL<V9990CmdEngine::V9990Bpp16>(EmuTime limit)
{
	unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = V9990Bpp16::getLogOpLUT(LOG);
	const bool timed = accessTimed();

	// a source word and a destination word
	while (step(limit, timed, 4, Tm::WORD_LMMM[Tm::BPP16])) {
		auto src = uint16_t(vram.readVRAMBx(srcAddress + 0) +
		                    vram.readVRAMBx(srcAddress + 1) * 256);
		srcAddress += 2;
		V9990Bpp16::pset(vram, DX, DY, pitch, src, WM, lut, LOG, dataLatch);
		DX += dx;
		if (!--ANX) {
			DX -= uint16_t(NX * dx);
			DY += dy;
			if (!--ANY) {
				endRect();
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeBMXL(EmuTime limit)
{
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	const unsigned npw = Tm::PIXELS_PER_WORD[m];

	while (true) {
		// a source byte (half a word), and the destination words its
		// pixels start
		unsigned accesses =
			1 + 2 * wordStarts(DX, dx, npw, Mode::PIXELS_PER_BYTE);
		if (!step(limit, timed, accesses, Tm::BYTE_BMXL[m])) break;
		uint8_t d = vram.readVRAMBx(srcAddress++);
		for (int i = 0; (ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			auto d2 = Mode::shift(d, i, DX);
			Mode::pset(vram, DX, DY, pitch, d2, WM, lut, LOG, dataLatch);
			DX += dx;
			if (!--ANX) {
				DX -= uint16_t(NX * dx);
				DY += dy;
				if (!--ANY) {
					endRect();
					cmdReady(engineTime);
					return;
				} else {
					ANX = getWrappedNX();
				}
			}
		}
	}
}

// BMLX
void V9990CmdEngine::startBMLX(EmuTime time)
{
	engineTime = time;
	dstAddress = (DX & 0xFF) + ((DY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<>
void V9990CmdEngine::executeBMLX<V9990CmdEngine::V9990Bpp16>(EmuTime limit)
{
	// TODO test corner cases, timing
	unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	const bool timed = accessTimed();

	while (true) {
		// a source word and a destination word
		bool rowStart = ANX == getWrappedNX();
		int think = Tm::WORD_LMMM[Tm::BPP16] +
		            (rowStart ? Tm::ROW_LMMM[Tm::BPP16] : 0);
		if (!step(limit, timed, 4, think)) break;
		auto src = V9990Bpp16::point(vram, SX, SY, pitch);
		vram.writeVRAMBx(dstAddress++, narrow_cast<uint8_t>(src & 0xFF));
		vram.writeVRAMBx(dstAddress++, narrow_cast<uint8_t>(src >> 8));
		SX += dx;
		if (!--ANX) {
			SX -= uint16_t(NX * dx);
			SY += dy;
			if (!--ANY) {
				endRect();
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}
template<typename Mode>
void V9990CmdEngine::executeBMLX(EmuTime limit)
{
	// TODO test corner cases, timing
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	uint16_t dx = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t dy = (ARG & DIY) ? uint16_t(-1) : 1;
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	const unsigned npw = Tm::PIXELS_PER_WORD[m];

	while (true) {
		// the source words a byte's pixels start, and a destination
		// byte (half a word)
		bool rowStart = ANX == getWrappedNX();
		unsigned accesses =
			2 * wordStarts(SX, dx, npw, Mode::PIXELS_PER_BYTE) + 1;
		int think = Tm::BYTE_BMLX[m] + (rowStart ? Tm::rowBMLX(m) : 0);
		if (!step(limit, timed, accesses, think)) break;
		uint8_t d = 0;
		for (auto i : xrange(Mode::PIXELS_PER_BYTE)) {
			auto src = Mode::point(vram, SX, SY, pitch);
			d |= uint8_t(Mode::shift(src, SX, i) & Mode::shiftMask(i));
			SX += dx;
			if (!--ANX) {
				SX -= uint16_t(NX * dx);
				SY += dy;
				if (!--ANY) {
					vram.writeVRAMBx(dstAddress++, d);
					endRect();
					cmdReady(engineTime);
					return;
				} else {
					ANX = getWrappedNX();
				}
			}
		}
		vram.writeVRAMBx(dstAddress++, d);
	}
}

// BMLL
void V9990CmdEngine::startBMLL(EmuTime time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	dstAddress = (DX & 0xFF) + ((DY & 0x7FF) << 8);
	nbBytes    = (NX & 0xFF) + ((NY & 0x7FF) << 8);
	if (nbBytes == 0) {
		nbBytes = 0x80000;
	}
}
void V9990CmdEngine::startBMLL16(EmuTime time)
{
	startBMLL(time);
	// TODO is this correct???
	// drop last bit
	srcAddress >>= 1;
	dstAddress >>= 1;
	nbBytes    >>= 1;
}

template<>
void V9990CmdEngine::executeBMLL<V9990CmdEngine::V9990Bpp16>(EmuTime limit)
{
	// TODO DIX DIY?
	auto lut = V9990Bpp16::getLogOpLUT(LOG);
	bool transp = (LOG & 0x10) != 0;
	const bool timed = accessTimed();
	while (true) {
		// a source word and a destination word
		if (!step(limit, timed, 4, 2 * Tm::BYTE_BMLL[Tm::BPP16])) break;
		// VRAM always mapped as in Bx modes
		auto srcColor = uint16_t(vram.readVRAMDirect(srcAddress + 0x00000) +
		                         vram.readVRAMDirect(srcAddress + 0x40000) * 256);
		auto dstColor = uint16_t(vram.readVRAMDirect(dstAddress + 0x00000) +
		                         vram.readVRAMDirect(dstAddress + 0x40000) * 256);
		uint16_t newColor = V9990Bpp16::logOp(lut, srcColor, dstColor, transp);
		uint16_t result = (dstColor & ~WM) | (newColor & WM);
		vram.writeVRAMDirect(dstAddress + 0x00000, narrow_cast<uint8_t>(result & 0xFF));
		vram.writeVRAMDirect(dstAddress + 0x40000, narrow_cast<uint8_t>(result >> 8));
		srcAddress = (srcAddress + 1) & 0x3FFFF;
		dstAddress = (dstAddress + 1) & 0x3FFFF;
		if (!--nbBytes) {
			endBMLL();
			cmdReady(engineTime);
			return;
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeBMLL(EmuTime limit)
{
	// TODO DIX DIY?
	auto lut = Mode::getLogOpLUT(LOG);
	const bool timed = accessTimed();
	const unsigned m = timingMode();
	while (true) {
		// Half a source word and half a destination word. It takes
		// longer if the two bytes are in different chips.
		bool cross = ((srcAddress ^ dstAddress) & 1) != 0;
		int think = Tm::BYTE_BMLL[m] + (cross ? Tm::CROSS_BMLL : 0);
		if (!step(limit, timed, 2, think)) break;
		// VRAM always mapped as in Bx modes
		auto srcColor = vram.readVRAMBx(srcAddress);
		auto addr = V9990VRAM::transformBx(dstAddress);
		auto dstColor = vram.readVRAMDirect(addr);
		auto newColor = Mode::logOp(lut, srcColor, dstColor);
		auto mask = narrow_cast<uint8_t>((addr & 0x40000) ? (WM >> 8) : (WM & 0xFF));
		uint8_t result = (dstColor & ~mask) | (newColor & mask);
		vram.writeVRAMDirect(addr, result);
		srcAddress = (srcAddress + 1) & 0x7FFFF;
		dstAddress = (dstAddress + 1) & 0x7FFFF;
		if (!--nbBytes) {
			endBMLL();
			cmdReady(engineTime);
			return;
		}
	}
}

// LINE
void V9990CmdEngine::startLINE(EmuTime time)
{
	engineTime = time;
	// Measured with MJ 3, MI 1: the first Y step comes after the first
	// pixel, one pixel earlier than with (NX - 1) / 2. This is the same
	// for even MJ, earlier for odd MJ. (An alternative that also fits:
	// (NX - 1) / 2 with 'ASX <= NY' below; MJ 4, MI 1 tells them apart.)
	ASX = uint16_t(std::max<unsigned>(NX, 2) / 2 - 1);
	ADX = DX;
	ANX = 0;
}

template<typename Mode>
void V9990CmdEngine::executeLINE(EmuTime limit)
{
	unsigned width = vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);

	uint16_t TX = (ARG & DIX) ? uint16_t(-1) : 1;
	uint16_t TY = (ARG & DIY) ? uint16_t(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	const bool timed = accessTimed();
	const unsigned m = timingMode();

	if ((ARG & MAJ) == 0) {
		// X-Axis is major direction.
		while (step(limit, timed, 2, Tm::PIXEL_LINE[m])) {
			Mode::psetColor(vram, ADX, DY, pitch, fgCol, WM, lut, LOG, dataLatch);

			ADX += TX;
			if (ASX < NY) {
				ASX += NX;
				DY += TY;
			}
			ASX -= NY;
			//ASX &= 1023; // mask to 10 bits range
			if (ANX++ == NX || (ADX & width)) {
				SX = 0; // SX is the X counter
				cmdReady(engineTime); // LINE leaves NY alone
				break;
			}
		}
	} else {
		// Y-Axis is major direction.
		while (step(limit, timed, 2, Tm::PIXEL_LINE[m])) {
			Mode::psetColor(vram, ADX, DY, pitch, fgCol, WM, lut, LOG, dataLatch);
			DY += TY;
			if (ASX < NY) {
				ASX += NX;
				ADX += TX;
			}
			ASX -= NY;
			//ASX &= 1023; // mask to 10 bits range
			if (ANX++ == NX || (ADX & width)) {
				SX = 0; // SX is the X counter
				cmdReady(engineTime); // LINE leaves NY alone
				break;
			}
		}
	}
}

// SRCH
void V9990CmdEngine::startSRCH(EmuTime time)
{
	engineTime = time;
	ASX = SX;
}

template<typename Mode>
void V9990CmdEngine::executeSRCH(EmuTime limit)
{
	using Type = typename Mode::Type;
	auto delta = getTiming(*this, SRCH_TIMING);
	unsigned width = vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);
	Type mask = (1 << Mode::BITS_PER_PIXEL) -1;

	uint16_t TX = (ARG & DIX) ? uint16_t(-1) : 1;
	bool AEQ = (ARG & NEQ) != 0;

	while (engineTime < limit) {
		engineTime += delta;
		Type value;
		Type col;
		Type mask2;
		if constexpr (Mode::BITS_PER_PIXEL == 16) {
			value = Mode::point(vram, ASX, SY, pitch);
			col = static_cast<Type>(fgCol);
			mask2 = static_cast<Type>(~0);
		} else {
			// TODO check
			unsigned addr = Mode::addressOf(ASX, SY, pitch);
			value = vram.readVRAMDirect(addr);
			col = narrow_cast<uint8_t>((addr & 0x40000) ? (fgCol >> 8) : (fgCol & 0xFF));
			mask2 = Mode::shift(mask, 3, ASX);
		}
		if (((value & mask2) == (col & mask2)) ^ AEQ) {
			status |= BD; // border detected
			endSRCH();
			cmdReady(engineTime);
			break;
		}
		ASX += TX;
		if (ASX & width) {
			status &= ~BD; // border not detected
			endSRCH();
			cmdReady(engineTime);
			break;
		}
	}
}

// POINT
template<typename Mode>
void V9990CmdEngine::startPOINT(EmuTime /*time*/)
{
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	auto d = Mode::point(vram, SX, SY, pitch);
	// Measured: POINT ends like an LMCM of one row: SY steps by one, DX
	// (the X counter) ends at 0 and NY is decremented twice.
	SY = uint16_t((SY + ((ARG & DIY) ? -1 : 1)) & 0xFFF);
	DX = 0;
	NY = uint16_t((NY - 1) & 0xFFF);
	endDecNY();

	if constexpr (Mode::BITS_PER_PIXEL != 16) {
		data = uint8_t(d);
		endAfterRead = true;
	} else {
		unsigned tmp = d; // workaround for VC++ warning C4333
				  // (in case Mode::Type == uint8_t and
				  //          Mode::BITS_PER_PIXEL == 8)
		data = narrow_cast<uint8_t>(tmp & 0xff);
		partial = narrow_cast<uint8_t>(tmp >> 8);
		endAfterRead = false;
	}
	status |= TR;
}

template<typename Mode>
void V9990CmdEngine::executePOINT(EmuTime /*limit*/)
{
	if (status & TR) return;

	assert(Mode::BITS_PER_PIXEL == 16);
	status |= TR;
	data = partial;
	endAfterRead = true;
}

// PSET
template<typename Mode>
void V9990CmdEngine::startPSET(EmuTime time)
{
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	auto lut = Mode::getLogOpLUT(LOG);
	Mode::psetColor(vram, DX, DY, pitch, fgCol, WM, lut, LOG, dataLatch);
	advancePointer();
	cmdReady(time);
}

void V9990CmdEngine::executePSET(EmuTime /*limit*/)
{
	UNREACHABLE;
}

// ADVN
void V9990CmdEngine::startADVN(EmuTime time)
{
	advancePointer();
	cmdReady(time);
}

// End of PSET and ADVN, measured in P1: AYE (R#52 bit 2) steps DY by +1, or
// by -1 with AYM (bit 3) too. AXE/AXM (bits 1-0) change nothing that was
// seen (DX stays). SX ends at 0 and NY is decremented once.
// TODO: what AXE/AXM move.
void V9990CmdEngine::advancePointer()
{
	if (CMD & 0x04) {
		DY = uint16_t((DY + ((CMD & 0x08) ? -1 : 1)) & 0xFFF);
	}
	SX = 0;
	endDecNY();
}

void V9990CmdEngine::executeADVN(EmuTime /*limit*/)
{
	UNREACHABLE;
}

// ====================================================================
// CmdEngine methods

void V9990CmdEngine::sync2(EmuTime time)
{
	switch (cmdMode | (CMD >> 4)) {
		case 0x00: case 0x10: case 0x20: case 0x30: case 0x40: case 0x50:
			executeSTOP(time); break;

		case 0x01: executeLMMC<V9990P1   >(time); break;
		case 0x11: executeLMMC<V9990P2   >(time); break;
		case 0x21: executeLMMC<V9990Bpp2 >(time); break;
		case 0x31: executeLMMC<V9990Bpp4 >(time); break;
		case 0x41: executeLMMC<V9990Bpp8 >(time); break;
		case 0x51: executeLMMC<V9990Bpp16>(time); break;

		case 0x02: executeLMMV<V9990P1   >(time); break;
		case 0x12: executeLMMV<V9990P2   >(time); break;
		case 0x22: executeLMMV<V9990Bpp2 >(time); break;
		case 0x32: executeLMMV<V9990Bpp4 >(time); break;
		case 0x42: executeLMMV<V9990Bpp8 >(time); break;
		case 0x52: executeLMMV<V9990Bpp16>(time); break;

		case 0x03: executeLMCM<V9990P1   >(time); break;
		case 0x13: executeLMCM<V9990P2   >(time); break;
		case 0x23: executeLMCM<V9990Bpp2 >(time); break;
		case 0x33: executeLMCM<V9990Bpp4 >(time); break;
		case 0x43: executeLMCM<V9990Bpp8 >(time); break;
		case 0x53: executeLMCM<V9990Bpp16>(time); break;

		case 0x04: executeLMMM<V9990P1   >(time); break;
		case 0x14: executeLMMM<V9990P2   >(time); break;
		case 0x24: executeLMMM<V9990Bpp2 >(time); break;
		case 0x34: executeLMMM<V9990Bpp4 >(time); break;
		case 0x44: executeLMMM<V9990Bpp8 >(time); break;
		case 0x54: executeLMMM<V9990Bpp16>(time); break;

		case 0x05: executeCMMC<V9990P1   >(time); break;
		case 0x15: executeCMMC<V9990P2   >(time); break;
		case 0x25: executeCMMC<V9990Bpp2 >(time); break;
		case 0x35: executeCMMC<V9990Bpp4 >(time); break;
		case 0x45: executeCMMC<V9990Bpp8 >(time); break;
		case 0x55: executeCMMC<V9990Bpp16>(time); break;

		case 0x06: case 0x16: case 0x26: case 0x36: case 0x46: case 0x56:
			executeCMMK(time); break;

		case 0x07: executeCMMM<V9990P1   >(time); break;
		case 0x17: executeCMMM<V9990P2   >(time); break;
		case 0x27: executeCMMM<V9990Bpp2 >(time); break;
		case 0x37: executeCMMM<V9990Bpp4 >(time); break;
		case 0x47: executeCMMM<V9990Bpp8 >(time); break;
		case 0x57: executeCMMM<V9990Bpp16>(time); break;

		case 0x08: executeBMXL<V9990P1   >(time); break;
		case 0x18: executeBMXL<V9990P2   >(time); break;
		case 0x28: executeBMXL<V9990Bpp2 >(time); break;
		case 0x38: executeBMXL<V9990Bpp4 >(time); break;
		case 0x48: executeBMXL<V9990Bpp8 >(time); break;
		case 0x58: executeBMXL<V9990Bpp16>(time); break;

		case 0x09: executeBMLX<V9990P1   >(time); break;
		case 0x19: executeBMLX<V9990P2   >(time); break;
		case 0x29: executeBMLX<V9990Bpp2 >(time); break;
		case 0x39: executeBMLX<V9990Bpp4 >(time); break;
		case 0x49: executeBMLX<V9990Bpp8 >(time); break;
		case 0x59: executeBMLX<V9990Bpp16>(time); break;

		case 0x0A: executeBMLL<V9990P1   >(time); break;
		case 0x1A: executeBMLL<V9990P2   >(time); break;
		case 0x2A: executeBMLL<V9990Bpp2 >(time); break;
		case 0x3A: executeBMLL<V9990Bpp4 >(time); break;
		case 0x4A: executeBMLL<V9990Bpp8 >(time); break;
		case 0x5A: executeBMLL<V9990Bpp16>(time); break;

		case 0x0B: executeLINE<V9990P1   >(time); break;
		case 0x1B: executeLINE<V9990P2   >(time); break;
		case 0x2B: executeLINE<V9990Bpp2 >(time); break;
		case 0x3B: executeLINE<V9990Bpp4 >(time); break;
		case 0x4B: executeLINE<V9990Bpp8 >(time); break;
		case 0x5B: executeLINE<V9990Bpp16>(time); break;

		case 0x0C: executeSRCH<V9990P1   >(time); break;
		case 0x1C: executeSRCH<V9990P2   >(time); break;
		case 0x2C: executeSRCH<V9990Bpp2 >(time); break;
		case 0x3C: executeSRCH<V9990Bpp4 >(time); break;
		case 0x4C: executeSRCH<V9990Bpp8 >(time); break;
		case 0x5C: executeSRCH<V9990Bpp16>(time); break;

		case 0x0D: executePOINT<V9990P1   >(time); break;
		case 0x1D: executePOINT<V9990P2   >(time); break;
		case 0x2D: executePOINT<V9990Bpp2 >(time); break;
		case 0x3D: executePOINT<V9990Bpp4 >(time); break;
		case 0x4D: executePOINT<V9990Bpp8 >(time); break;
		case 0x5D: executePOINT<V9990Bpp16>(time); break;

		case 0x0E: case 0x1E: case 0x2E: case 0x3E: case 0x4E: case 0x5E:
			executePSET(time); break;

		case 0x0F: case 0x1F: case 0x2F: case 0x3F: case 0x4F: case 0x5F:
			executeADVN(time); break;

		default: UNREACHABLE;
	}
}

void V9990CmdEngine::setCmdData(uint8_t value, EmuTime time)
{
	sync(time);
	data = value;
	status &= ~TR;
}

uint8_t V9990CmdEngine::getCmdData(EmuTime time)
{
	sync(time);

	uint8_t value = 0xFF;
	if (status & TR) {
		value = data;
		status &= ~TR;
		if (endAfterRead) {
			endAfterRead = false;
			cmdReady(time);
		}
	}
	return value;
}

uint8_t V9990CmdEngine::peekCmdData(EmuTime time) const
{
	const_cast<V9990CmdEngine*>(this)->sync(time);
	return (status & TR) ? data : 0xFF;
}

// The command registers after a command, as measured on a real V9990 in P1
// (R#32-R#52 are write only; a program sees them when it starts the next
// command without rewriting them all). The engine counts with the registers
// themselves: NY is the row counter, and every command but LINE decrements
// it once more at the end (STOP too); SX is the X counter of commands that
// don't read a source (LMMC, LMMV, CMMC, LINE, PSET, ADVN), DX that of
// commands that don't draw (LMCM, POINT, SRCH). Both end at 0.
void V9990CmdEngine::endDecNY()
{
	NY = uint16_t((NY - 1) & 0xFFF);
}

// End of a rectangle command: NY counted down to 0, then once more.
void V9990CmdEngine::endRect()
{
	NY = 0xFFF;
}

// BMLL: SY:SX and DY:DX are linear addresses, the copy steps them by NY
// rows of 256 bytes (the low byte back at the start).
void V9990CmdEngine::endBMLL()
{
	SY = uint16_t((SY + NY) & 0xFFF);
	DY = uint16_t((DY + NY) & 0xFFF);
	endRect();
}

// SRCH: SX is its X pointer and ends where the search stopped (the found
// X, or one past the edge); DX ends at 0.
void V9990CmdEngine::endSRCH()
{
	SX = uint16_t(ASX & 0x7FF);
	DX = 0;
	endDecNY();
}

void V9990CmdEngine::cmdReady(EmuTime /*time*/)
{
	CMD = 0; // for deserialize
	status &= ~(CE | TR);
	vdp.cmdReady();
}

EmuTime V9990CmdEngine::estimateCmdEnd() const
{
	// instantaneous (broken) timing needs no extra sync
	if (!accessTimed()) return engineTime;

	// A lower bound: when the pending step ends (exact), plus the least
	// the remaining steps can take. Each sync gets closer, and the last
	// step's end is exact.
	int64_t rect = ANX + int64_t(ANY - 1) * getWrappedNX();
	int64_t left = 0; // in Tm's time units
	switch (CMD >> 4) {
	case 0x02: // LMMV
	case 0x07: // CMMM
		left = rect * Tm::MIN_PIXEL_LMMV;
		break;
	case 0x04: // LMMM
	case 0x08: // BMXL
	case 0x09: // BMLX
		left = rect * Tm::MIN_PIXEL_COPY;
		break;
	case 0x0A: // BMLL
		left = int64_t(nbBytes) * Tm::MIN_BYTE_BMLL;
		break;
	case 0x0B: // LINE
		left = int64_t(NX - ANX) * Tm::MIN_PIXEL_LINE;
		break;
	// (not yet timed by accesses)
	case 0x0C: // SRCH: can end at any next pixel
		return engineTime + getTiming(*this, SRCH_TIMING);
	default:
		return engineTime; // ends at once, or when the CPU moves data
	}
	// (the pending step may already cover some of that)
	left = std::max<int64_t>(0, left - Tm::MAX_PENDING);
	return std::max(nextDone, engineTime) + Tm::dur(left);
}

// version 1: initial version
// version 2: we forgot to serialize the time (or clock) member
// version 3: added dataLatch (P1 data output register)
// version 4: removed borderX (R#53-R#54 read SX)
template<typename Archive>
void V9990CmdEngine::serialize(Archive& ar, unsigned version)
{
	// note: V9990Cmd objects are stateless
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("time", engineTime);
	} else {
		// In version 1 we forgot to serialize the time member (it was
		// also still a Clock back then). The best we can do is
		// initialize it with the current time, that's already done in
		// the constructor.
	}
	ar.serialize("srcAddress",   srcAddress,
	             "dstAddress",   dstAddress,
	             "nbBytes",      nbBytes,
	             "ASX",          ASX,
	             "ADX",          ADX,
	             "ANX",          ANX,
	             "ANY",          ANY,
	             "SX",           SX,
	             "SY",           SY,
	             "DX",           DX,
	             "DY",           DY,
	             "NX",           NX,
	             "NY",           NY,
	             "WM",           WM,
	             "fgCol",        fgCol,
	             "bgCol",        bgCol,
	             "ARG",          ARG,
	             "LOG",          LOG,
	             "CMD",          CMD,
	             "status",       status,
	             "data",         data,
	             "bitsLeft",     bitsLeft,
	             "partial",      partial,
	             "endAfterRead", endAfterRead);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("dataLatch", dataLatch);
	} else {
		dataLatch = 0;
	}
	if constexpr (Archive::IS_LOADER) {
		setCommandMode();
		nextDone = engineTime; // (a lower bound)
	}
}
INSTANTIATE_SERIALIZE_METHODS(V9990CmdEngine);

} // namespace openmsx
