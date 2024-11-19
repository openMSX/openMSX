#include "V9990CmdEngine.hh"

#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990DisplayTiming.hh"
#include "MSXMotherBoard.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "MemBuffer.hh"
#include "Clock.hh"
#include "serialize.hh"

#include "checked_cast.hh"
#include "narrow.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <array>
#include <cassert>
#include <iostream>
#include <string_view>

namespace openmsx {

static constexpr unsigned maxLength = 171; // The maximum value from the xxx_TIMING tables below
static constexpr EmuDuration d_(unsigned x)
{
	assert(x <= maxLength);
	return Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND>::duration(x);
}
using EDStorage = EmuDurationStorageFor<d_(maxLength).length()>;
static constexpr EDStorage d(unsigned x) { return EDStorage{d_(x)}; }
using A  = std::array<const EDStorage, 4>;
using A2 = std::array<const A, 3>;
using TimingTable = std::span<const A2, 4>;

// 1st index  B0/2/4, B1/3/7, P1, P2
// 2nd index  sprites-ON, sprites-OFF, display-OFF
// 3th index  2bpp, 4bpp, 8bpp, 16bpp
//            (for P1/P2 fill in the same value 4 times)
static constexpr std::array LMMV_TIMING = {
	A2{A{ d( 8), d(11), d(15), d(30)}, A{d( 7), d(10), d(13), d(26)}, A{d( 7), d(10), d(13), d(25)}},
	A2{A{ d( 5), d( 7), d( 9), d(18)}, A{d( 5), d( 6), d( 8), d(17)}, A{d( 5), d( 6), d( 8), d(17)}},
	A2{A{ d(56), d(56), d(56), d(56)}, A{d(25), d(25), d(25), d(25)}, A{d( 9), d( 9), d( 9), d( 9)}},
	A2{A{ d(28), d(28), d(28), d(28)}, A{d(15), d(15), d(15), d(15)}, A{d( 6), d( 6), d( 6), d( 6)}}
};
static constexpr std::array LMMM_TIMING = {
	A2{A{d (10),d (16),d( 32),d( 66)}, A{d( 8), d(14), d(28), d(57)}, A{d( 8), d(13), d(27), d(54)}},
	A2{A{d ( 6),d( 10),d( 20),d( 39)}, A{d( 5), d( 9), d(18), d(35)}, A{d( 5), d( 9), d(17), d(35)}},
	A2{A{d(115),d(115),d(115),d(115)}, A{d(52), d(52), d(52), d(52)}, A{d(18), d(18), d(18), d(18)}},
	A2{A{d( 57),d( 57),d( 57),d( 57)}, A{d(25), d(25), d(25), d(25)}, A{d( 9), d( 9), d( 9), d( 9)}}
};
static constexpr std::array BMXL_TIMING = { // NOTE: values are BYTE based here!
	A2{A{d( 38),d( 33),d( 32),d( 33)}, A{d(33), d(28), d(28), d(28)}, A{d(33), d(27), d(27), d(27)}}, // identical to LMMM (b)
	A2{A{d( 24),d( 20),d( 20),d (19)}, A{d(22), d(18), d(18), d(18)}, A{d(21), d(17), d(17), d(17)}}, // identical to LMMM (b)
	A2{A{d(171),d(171),d(171),d(171)}, A{d(82), d(82), d(82), d(82)}, A{d(29), d(29), d(29), d(29)}},
	A2{A{d(114),d(114),d(114),d(114)}, A{d(50), d(50), d(50), d(50)}, A{d(18), d(18), d(18), d(18)}}
};
static constexpr std::array BMLX_TIMING = {
	A2{A{ d(10), d(16), d(32), d(66)}, A{d( 8), d(14), d(28), d(57)}, A{d( 8), d(13), d(27), d(54)}}, // identical to LMMM
	A2{A{ d( 6), d(10), d(20), d(39)}, A{d( 5), d( 9), d(18), d(35)}, A{d( 5), d( 9), d(17), d(35)}}, // identical to LMMM
	A2{A{ d(84), d(84), d(84), d(84)}, A{d(44), d(44), d(44), d(44)}, A{d(17), d(17), d(17), d(17)}},
	A2{A{ d(57), d(57), d(57), d(57)}, A{d(25), d(25), d(25), d(25)}, A{d( 9), d( 9), d( 9), d( 9)}}
};
static constexpr std::array BMLL_TIMING = {
	A2{A{d( 33),d( 33),d( 33),d( 33)}, A{d(28), d(28), d(28), d(28)}, A{d(27), d(27), d(27), d(27)}},
	A2{A{d( 20),d( 20),d( 20),d( 20)}, A{d(18), d(18), d(18), d(18)}, A{d(18), d(18), d(18), d(18)}},
	A2{A{d(118),d(118),d(118),d(118)}, A{d(52), d(52), d(52), d(52)}, A{d(18), d(18), d(18), d(18)}},
	A2{A{d(118),d(118),d(118),d(118)}, A{d(52), d(52), d(52), d(52)}, A{d(18), d(18), d(18), d(18)}}
};
static constexpr std::array CMMM_TIMING = {
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}  // TODO
};
static constexpr std::array LINE_TIMING = {
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}, // TODO
	A2{A{ d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}, A{d(24), d(24), d(24), d(24)}}  // TODO
};
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



// Lazily initialized LUT to speed up logical operations:
//  - 1st index is the mode: 2,4,8 bpp or 'not-transparent'
//  - 2nd index is the logical operation: one of the 16 possible binary functions
// * Each entry contains a 256x256 byte array, that array is indexed using
//   destination and source byte (in that order).
// * A fully populated logOpLUT would take 4MB, however the vast majority of
//   this table is (almost) never used. So we save quite some memory (and
//   startup time) by lazily initializing this table.
enum class Log {
	NO_T, BPP2, BPP4, BPP8,
	NUM
};
static array_with_enum_index<Log, std::array<MemBuffer<byte>, 16>> logOpLUT;

// to speedup calculating logOpLUT
static constexpr auto bitLUT = [] {
	std::array<std::array<std::array<std::array<byte, 2>, 2>, 16>, 8> result = {};
	for (auto op : xrange(16)) {
		unsigned tmp = op;
		for (auto src : xrange(2)) {
			for (auto dst : xrange(2)) {
				unsigned b = tmp & 1;
				for (auto bit : xrange(8)) {
					result[bit][op][src][dst] = narrow<byte>(b << bit);
				}
				tmp >>= 1;
			}
		}
	}
	return result;
}();

[[nodiscard]] static constexpr byte func01(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x03) == 0) return dst & 0x03;
	byte res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	return res;
}
[[nodiscard]] static constexpr byte func23(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0C) == 0) return dst & 0x0C;
	byte res = 0;
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
[[nodiscard]] static constexpr byte func45(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x30) == 0) return dst & 0x30;
	byte res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	return res;
}
[[nodiscard]] static constexpr byte func67(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xC0) == 0) return dst & 0xC0;
	byte res = 0;
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

[[nodiscard]] static constexpr byte func03(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0F) == 0) return dst & 0x0F;
	byte res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
[[nodiscard]] static constexpr byte func47(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xF0) == 0) return dst & 0xF0;
	byte res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

[[nodiscard]] static constexpr byte func07(unsigned op, unsigned src, unsigned dst)
{
	// if (src == 0) return dst;  // handled in fillTable8
	byte res = 0;
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

static constexpr void fillTableNoT(unsigned op, std::span<byte, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

static constexpr void fillTable2(unsigned op, std::span<byte, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			byte res = 0;
			res |= func01(op, src, dst);
			res |= func23(op, src, dst);
			res |= func45(op, src, dst);
			res |= func67(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static constexpr void fillTable4(unsigned op, std::span<byte, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		for (auto src : xrange(256)) {
			byte res = 0;
			res |= func03(op, src, dst);
			res |= func47(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static constexpr void fillTable8(unsigned op, std::span<byte, 256 * 256> table)
{
	for (auto dst : xrange(256)) {
		{ // src == 0
			table[dst * 256 + 0  ] = narrow_cast<byte>(dst);
		}
		for (auto src : xrange(1, 256)) { // src != 0
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

[[nodiscard]] static std::span<const byte, 256 * 256> getLogOpImpl(Log mode, unsigned op)
{
	op &= 0x0f;
	auto& lut = logOpLUT[mode][op];
	if (!lut.data()) {
		lut.resize(256 * 256);
		std::span<byte, 256 * 256> s{lut};
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
	return std::span<byte, 256 * 256>{lut};
}


static constexpr byte DIY = 0x08;
static constexpr byte DIX = 0x04;
static constexpr byte NEQ = 0x02;
static constexpr byte MAJ = 0x01;

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

inline byte V9990CmdEngine::V9990P1::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990P1::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? byte(value >> shift) : byte(value << -shift);
}

inline byte V9990CmdEngine::V9990P1::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990P1::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline byte V9990CmdEngine::V9990P1::logOp(
	std::span<const byte, 256 * 256> lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990P1::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}
inline void V9990CmdEngine::V9990P1::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = narrow_cast<byte>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & (0xF0 >> (4 * (x & 1)));
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
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

inline byte V9990CmdEngine::V9990P2::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990P2::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? byte(value >> shift) : byte(value << -shift);
}

inline byte V9990CmdEngine::V9990P2::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990P2::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline byte V9990CmdEngine::V9990P2::logOp(
	std::span<const byte, 256 * 256> lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990P2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990P2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = narrow_cast<byte>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & (0xF0 >> (4 * (x & 1)));
	byte result = (dstColor & ~mask2) | (newColor & mask2);
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

inline byte V9990CmdEngine::V9990Bpp2::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990Bpp2::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 2 * (narrow<int>(toX & 3) - narrow<int>(fromX & 3));
	return (shift > 0) ? byte(value >> shift) : byte(value << -shift);
}

inline byte V9990CmdEngine::V9990Bpp2::shiftMask(unsigned x)
{
	return 0xC0 >> (2 * (x & 3));
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990Bpp2::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP2 : Log::NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp2::logOp(
	std::span<const byte, 256 * 256> lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = narrow_cast<byte>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & (0xC0 >> (2 * (x & 3)));
	byte result = (dstColor & ~mask2) | (newColor & mask2);
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

inline byte V9990CmdEngine::V9990Bpp4::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990Bpp4::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * (narrow<int>(toX & 1) - narrow<int>(fromX & 1));
	return (shift > 0) ? byte(value >> shift) : byte(value << -shift);
}

inline byte V9990CmdEngine::V9990Bpp4::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990Bpp4::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP4 : Log::NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp4::logOp(
	std::span<const byte, 256 * 256> lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp4::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp4::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = narrow_cast<byte>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte mask2 = mask1 & (0xF0 >> (4 * (x & 1)));
	byte result = (dstColor & ~mask2) | (newColor & mask2);
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

inline byte V9990CmdEngine::V9990Bpp8::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990Bpp8::shift(
	byte value, unsigned /*fromX*/, unsigned /*toX*/)
{
	return value;
}

inline byte V9990CmdEngine::V9990Bpp8::shiftMask(unsigned /*x*/)
{
	return 0xFF;
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990Bpp8::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? Log::BPP8 : Log::NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp8::logOp(
	std::span<const byte, 256 * 256> lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp8::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte result = (dstColor & ~mask1) | (newColor & mask1);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp8::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, std::span<const byte, 256 * 256> lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = narrow_cast<byte>((addr & 0x40000) ? (color >> 8) : (color & 0xFF));
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = narrow_cast<byte>((addr & 0x40000) ? (mask >> 8) : (mask & 0xFF));
	byte result = (dstColor & ~mask1) | (newColor & mask1);
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

inline word V9990CmdEngine::V9990Bpp16::point(
	const V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	unsigned addr = addressOf(x, y, pitch);
	return word(vram.readVRAMDirect(addr + 0x00000) +
	            vram.readVRAMDirect(addr + 0x40000) * 256);
}

inline word V9990CmdEngine::V9990Bpp16::shift(
	word value, unsigned /*fromX*/, unsigned /*toX*/)
{
	return value;
}

inline word V9990CmdEngine::V9990Bpp16::shiftMask(unsigned /*x*/)
{
	return 0xFFFF;
}

inline std::span<const byte, 256 * 256> V9990CmdEngine::V9990Bpp16::getLogOpLUT(byte op)
{
	return getLogOpImpl(Log::NO_T, op);
}

inline word V9990CmdEngine::V9990Bpp16::logOp(
	std::span<const byte, 256 * 256> lut, word src, word dst, bool transp)
{
	if (transp && (src == 0)) return dst;
	return word((lut[((dst & 0x00FF) << 8) + ((src & 0x00FF) >> 0)] << 0) +
	            (lut[((dst & 0xFF00) << 0) + ((src & 0xFF00) >> 8)] << 8));
}

inline void V9990CmdEngine::V9990Bpp16::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word srcColor, word mask, std::span<const byte, 256 * 256> lut, byte op)
{
	unsigned addr = addressOf(x, y, pitch);
	auto dstColor = word(vram.readVRAMDirect(addr + 0x00000) +
	                     vram.readVRAMDirect(addr + 0x40000) * 256);
	word newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	word result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, narrow_cast<byte>(result & 0xFF));
	vram.writeVRAMDirect(addr + 0x40000, narrow_cast<byte>(result >> 8));
}

inline void V9990CmdEngine::V9990Bpp16::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word srcColor, word mask, std::span<const byte, 256 * 256> lut, byte op)
{
	unsigned addr = addressOf(x, y, pitch);
	auto dstColor = word(vram.readVRAMDirect(addr + 0x00000) +
	                     vram.readVRAMDirect(addr + 0x40000) * 256);
	word newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	word result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, narrow_cast<byte>(result & 0xFF));
	vram.writeVRAMDirect(addr + 0x40000, narrow_cast<byte>(result >> 8));
}

// ====================================================================
/** Constructor
  */
V9990CmdEngine::V9990CmdEngine(V9990& vdp_, EmuTime::param time_,
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

void V9990CmdEngine::reset(EmuTime::param /*time*/)
{
	CMD = 0;
	status = 0;
	borderX = 0;
	endAfterRead = false;
}

void V9990CmdEngine::setCmdReg(byte reg, byte value, EmuTime::param time)
{
	sync(time);
	switch(reg - 32) {
	case  0: // SX low
		SX = word((SX & 0x0700) | ((value & 0xFF) << 0));
		break;
	case  1: // SX high
		SX = word((SX & 0x00FF) | ((value & 0x07) << 8));
		break;
	case  2: // SY low
		SY = word((SY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  3: // SY high
		SY = word((SY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case  4: // DX low
		DX = word((DX & 0x0700) | ((value & 0xFF) << 0));
		break;
	case  5: // DX high
		DX = word((DX & 0x00FF) | ((value & 0x07) << 8));
		break;
	case  6: // DY low
		DY = word((DY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  7: // DY high
		DY = word((DY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case  8: // NX low
		NX = word((NX & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case  9: // NX high
		NX = word((NX & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case 10: // NY low
		NY = word((NY & 0x0F00) | ((value & 0xFF) << 0));
		break;
	case 11: // NY high
		NY = word((NY & 0x00FF) | ((value & 0x0F) << 8));
		break;
	case 12: // ARG
		ARG = value & 0x0F;
		break;
	case 13: // LOGOP
		LOG = value & 0x1F;
		break;
	case 14: // write mask low
		WM = word((WM & 0xFF00) | (value << 0));
		break;
	case 15: // write mask high
		WM = word((WM & 0x00FF) | (value << 8));
		break;
	case 16: // Font color - FG low
		fgCol = word((fgCol & 0xFF00) | (value << 0));
		break;
	case 17: // Font color - FG high
		fgCol = word((fgCol & 0x00FF) | (value << 8));
		break;
	case 18: // Font color - BG low
		bgCol = word((bgCol & 0xFF00) | (value << 0));
		break;
	case 19: // Font color - BG high
		bgCol = word((bgCol & 0x00FF) | (value << 8));
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
				UNREACHABLE;
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
void V9990CmdEngine::startSTOP(EmuTime::param time)
{
	cmdReady(time);
}

void V9990CmdEngine::executeSTOP(EmuTime::param /*limit*/)
{
	UNREACHABLE;
}

// LMMC
void V9990CmdEngine::startLMMC(EmuTime::param /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status |= TR;
}
void V9990CmdEngine::startLMMC16(EmuTime::param time)
{
	bitsLeft = 1;
	startLMMC(time);
}

template<>
void V9990CmdEngine::executeLMMC<V9990CmdEngine::V9990Bpp16>(EmuTime::param limit)
{
	if (!(status & TR)) {
		status |= TR;
		if (bitsLeft) {
			bitsLeft = 0;
			partial = data;
		} else {
			bitsLeft = 1;
			auto value = word((data << 8) | partial);
			unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
			auto lut = V9990Bpp16::getLogOpLUT(LOG);
			V9990Bpp16::pset(vram, DX, DY, pitch, value, WM, lut, LOG);
			word dx = (ARG & DIX) ? word(-1) : 1;
			DX += dx;
			if (!--ANX) {
				word dy = (ARG & DIY) ? word(-1) : 1;
				DX -= word(NX * dx);
				DY += dy;
				if (!--ANY) {
					cmdReady(limit);
				} else {
					ANX = getWrappedNX();
				}
			}
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeLMMC(EmuTime::param limit)
{
	if (!(status & TR)) {
		status |= TR;
		unsigned pitch = Mode::getPitch(vdp.getImageWidth());
		auto lut = Mode::getLogOpLUT(LOG);
		for (int i = 0; (ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			byte d = Mode::shift(data, i, DX);
			Mode::pset(vram, DX, DY, pitch, d, WM, lut, LOG);

			word dx = (ARG & DIX) ? word(-1) : 1;
			DX += dx;
			if (!--ANX) {
				word dy = (ARG & DIY) ? word(-1) : 1;
				DX -= word(NX * dx);
				DY += dy;
				if (!--ANY) {
					cmdReady(limit);
				} else {
					ANX = NX;
				}
			}
		}
	}
}

// LMMV
void V9990CmdEngine::startLMMV(EmuTime::param time)
{
	engineTime = time;
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<typename Mode>
void V9990CmdEngine::executeLMMV(EmuTime::param limit)
{
	// TODO can be optimized a lot

	auto delta = getTiming(*this, LMMV_TIMING);
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	while (engineTime < limit) {
		engineTime += delta;
		Mode::psetColor(vram, DX, DY, pitch, fgCol, WM, lut, LOG);

		DX += dx;
		if (!--ANX) {
			DX -= word(NX * dx);
			DY += dy;
			if (!--ANY) {
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// LMCM
void V9990CmdEngine::startLMCM(EmuTime::param /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status &= ~TR;
	endAfterRead = false;
}
void V9990CmdEngine::startLMCM16(EmuTime::param time)
{
	bitsLeft = 0;
	startLMCM(time);
}

template<typename Mode>
void V9990CmdEngine::executeLMCM(EmuTime::param /*limit*/)
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
			d |= Type(Mode::shift(src, SX, i) & Mode::shiftMask(i));

			word dx = (ARG & DIX) ? word(-1) : 1;
			SX += dx;
			if (!--ANX) {
				word dy = (ARG & DIY) ? word(-1) : 1;
				SX -= word(NX * dx);
				SY += dy;
				if (!--ANY) {
					endAfterRead = true;
				} else {
					ANX = getWrappedNX();
				}
			}
		}
		if constexpr (Mode::BITS_PER_PIXEL == 16) {
			unsigned tmp = d; // workaround for VC++ warning C4333
			                  // (in case Mode::Type == byte and
			                  //          Mode::BITS_PER_PIXEL == 8)
			data = narrow_cast<byte>(tmp & 0xff);
			partial = narrow_cast<byte>(tmp >> 8);
			bitsLeft = 1;
		} else {
			data = byte(d);
		}
	}
}

// LMMM
void V9990CmdEngine::startLMMM(EmuTime::param time)
{
	engineTime = time;
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<typename Mode>
void V9990CmdEngine::executeLMMM(EmuTime::param limit)
{
	// TODO can be optimized a lot

	auto delta = getTiming(*this, LMMM_TIMING);
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	while (engineTime < limit) {
		engineTime += delta;
		auto src = Mode::point(vram, SX, SY, pitch);
		src = Mode::shift(src, SX, DX);
		Mode::pset(vram, DX, DY, pitch, src, WM, lut, LOG);

		DX += dx;
		SX += dx;
		if (!--ANX) {
			DX -= word(NX * dx);
			SX -= word(NX * dx);
			DY += dy;
			SY += dy;
			if (!--ANY) {
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// CMMC
void V9990CmdEngine::startCMMC(EmuTime::param /*time*/)
{
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	status |= TR;
}

template<typename Mode>
void V9990CmdEngine::executeCMMC(EmuTime::param limit)
{
	if (!(status & TR)) {
		status |= TR;

		unsigned pitch = Mode::getPitch(vdp.getImageWidth());
		word dx = (ARG & DIX) ? word(-1) : 1;
		word dy = (ARG & DIY) ? word(-1) : 1;
		auto lut = Mode::getLogOpLUT(LOG);
		for (auto i : xrange(8)) {
			(void)i;
			bool bit = (data & 0x80) != 0;
			data <<= 1;

			word src = bit ? fgCol : bgCol;
			Mode::psetColor(vram, DX, DY, pitch, src, WM, lut, LOG);

			DX += dx;
			if (!--ANX) {
				DX -= word(NX * dx);
				DY += dy;
				if (!--ANY) {
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
void V9990CmdEngine::startCMMK(EmuTime::param time)
{
	std::cout << "V9990: CMMK not yet implemented\n";
	cmdReady(time); // TODO dummy implementation
}

void V9990CmdEngine::executeCMMK(EmuTime::param /*limit*/)
{
	UNREACHABLE;
}

// CMMM
void V9990CmdEngine::startCMMM(EmuTime::param time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
	bitsLeft = 0;
}

template<typename Mode>
void V9990CmdEngine::executeCMMM(EmuTime::param limit)
{
	// TODO can be optimized a lot

	auto delta = getTiming(*this, CMMM_TIMING);
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);
	while (engineTime < limit) {
		engineTime += delta;
		if (!bitsLeft) {
			data = vram.readVRAMBx(srcAddress++);
			bitsLeft = 8;
		}
		--bitsLeft;
		bool bit = (data & 0x80) != 0;
		data <<= 1;

		word color = bit ? fgCol : bgCol;
		Mode::psetColor(vram, DX, DY, pitch, color, WM, lut, LOG);

		DX += dx;
		if (!--ANX) {
			DX -= word(NX * dx);
			DY += dy;
			if (!--ANY) {
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

// BMXL
void V9990CmdEngine::startBMXL(EmuTime::param time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<>
void V9990CmdEngine::executeBMXL<V9990CmdEngine::V9990Bpp16>(EmuTime::param limit)
{
	// timing value is times 2, because it does 2 bytes per iteration:
	auto delta = getTiming(*this, BMXL_TIMING) * 2;
	unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;
	auto lut = V9990Bpp16::getLogOpLUT(LOG);

	while (engineTime < limit) {
		engineTime += delta;
		auto src = word(vram.readVRAMBx(srcAddress + 0) +
		                vram.readVRAMBx(srcAddress + 1) * 256);
		srcAddress += 2;
		V9990Bpp16::pset(vram, DX, DY, pitch, src, WM, lut, LOG);
		DX += dx;
		if (!--ANX) {
			DX -= word(NX * dx);
			DY += dy;
			if (!--ANY) {
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeBMXL(EmuTime::param limit)
{
	auto delta = getTiming(*this, BMXL_TIMING);
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);

	while (engineTime < limit) {
		engineTime += delta;
		byte d = vram.readVRAMBx(srcAddress++);
		for (int i = 0; (ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			auto d2 = Mode::shift(d, i, DX);
			Mode::pset(vram, DX, DY, pitch, d2, WM, lut, LOG);
			DX += dx;
			if (!--ANX) {
				DX -= word(NX * dx);
				DY += dy;
				if (!--ANY) {
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
void V9990CmdEngine::startBMLX(EmuTime::param time)
{
	engineTime = time;
	dstAddress = (DX & 0xFF) + ((DY & 0x7FF) << 8);
	ANX = getWrappedNX();
	ANY = getWrappedNY();
}

template<>
void V9990CmdEngine::executeBMLX<V9990CmdEngine::V9990Bpp16>(EmuTime::param limit)
{
	// TODO test corner cases, timing
	auto delta = getTiming(*this, BMLX_TIMING);
	unsigned pitch = V9990Bpp16::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;

	while (engineTime < limit) {
		engineTime += delta;
		auto src = V9990Bpp16::point(vram, SX, SY, pitch);
		vram.writeVRAMBx(dstAddress++, narrow_cast<byte>(src & 0xFF));
		vram.writeVRAMBx(dstAddress++, narrow_cast<byte>(src >> 8));
		SX += dx;
		if (!--ANX) {
			SX -= word(NX * dx);
			SY += dy;
			if (!--ANY) {
				cmdReady(engineTime);
				return;
			} else {
				ANX = getWrappedNX();
			}
		}
	}
}
template<typename Mode>
void V9990CmdEngine::executeBMLX(EmuTime::param limit)
{
	// TODO test corner cases, timing
	auto delta = getTiming(*this, BMLX_TIMING);
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	word dx = (ARG & DIX) ? word(-1) : 1;
	word dy = (ARG & DIY) ? word(-1) : 1;

	while (engineTime < limit) {
		engineTime += delta;
		byte d = 0;
		for (auto i : xrange(Mode::PIXELS_PER_BYTE)) {
			auto src = Mode::point(vram, SX, SY, pitch);
			d |= byte(Mode::shift(src, SX, i) & Mode::shiftMask(i));
			SX += dx;
			if (!--ANX) {
				SX -= word(NX * dx);
				SY += dy;
				if (!--ANY) {
					vram.writeVRAMBx(dstAddress++, d);
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
void V9990CmdEngine::startBMLL(EmuTime::param time)
{
	engineTime = time;
	srcAddress = (SX & 0xFF) + ((SY & 0x7FF) << 8);
	dstAddress = (DX & 0xFF) + ((DY & 0x7FF) << 8);
	nbBytes    = (NX & 0xFF) + ((NY & 0x7FF) << 8);
	if (nbBytes == 0) {
		nbBytes = 0x80000;
	}
}
void V9990CmdEngine::startBMLL16(EmuTime::param time)
{
	startBMLL(time);
	// TODO is this correct???
	// drop last bit
	srcAddress >>= 1;
	dstAddress >>= 1;
	nbBytes    >>= 1;
}

template<>
void V9990CmdEngine::executeBMLL<V9990CmdEngine::V9990Bpp16>(EmuTime::param limit)
{
	// TODO DIX DIY?
	// timing value is times 2, because it does 2 bytes per iteration:
	auto delta = getTiming(*this, BMLL_TIMING) * 2;
	auto lut = V9990Bpp16::getLogOpLUT(LOG);
	bool transp = (LOG & 0x10) != 0;
	while (engineTime < limit) {
		engineTime += delta;
		// VRAM always mapped as in Bx modes
		auto srcColor = word(vram.readVRAMDirect(srcAddress + 0x00000) +
		                     vram.readVRAMDirect(srcAddress + 0x40000) * 256);
		auto dstColor = word(vram.readVRAMDirect(dstAddress + 0x00000) +
		                     vram.readVRAMDirect(dstAddress + 0x40000) * 256);
		word newColor = V9990Bpp16::logOp(lut, srcColor, dstColor, transp);
		word result = (dstColor & ~WM) | (newColor & WM);
		vram.writeVRAMDirect(dstAddress + 0x00000, narrow_cast<byte>(result & 0xFF));
		vram.writeVRAMDirect(dstAddress + 0x40000, narrow_cast<byte>(result >> 8));
		srcAddress = (srcAddress + 1) & 0x3FFFF;
		dstAddress = (dstAddress + 1) & 0x3FFFF;
		if (!--nbBytes) {
			cmdReady(engineTime);
			return;
		}
	}
}

template<typename Mode>
void V9990CmdEngine::executeBMLL(EmuTime::param limit)
{
	// TODO DIX DIY?
	auto delta = getTiming(*this, BMLL_TIMING);
	auto lut = Mode::getLogOpLUT(LOG);
	while (engineTime < limit) {
		engineTime += delta;
		// VRAM always mapped as in Bx modes
		byte srcColor = vram.readVRAMBx(srcAddress);
		unsigned addr = V9990VRAM::transformBx(dstAddress);
		byte dstColor = vram.readVRAMDirect(addr);
		byte newColor = Mode::logOp(lut, srcColor, dstColor);
		byte mask = narrow_cast<byte>((addr & 0x40000) ? (WM >> 8) : (WM & 0xFF));
		byte result = (dstColor & ~mask) | (newColor & mask);
		vram.writeVRAMDirect(addr, result);
		srcAddress = (srcAddress + 1) & 0x7FFFF;
		dstAddress = (dstAddress + 1) & 0x7FFFF;
		if (!--nbBytes) {
			cmdReady(engineTime);
			return;
		}
	}
}

// LINE
void V9990CmdEngine::startLINE(EmuTime::param time)
{
	engineTime = time;
	ASX = word((NX - 1) / 2);
	ADX = DX;
	ANX = 0;
}

template<typename Mode>
void V9990CmdEngine::executeLINE(EmuTime::param limit)
{
	auto delta = getTiming(*this, LINE_TIMING);
	unsigned width = vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);

	word TX = (ARG & DIX) ? word(-1) : 1;
	word TY = (ARG & DIY) ? word(-1) : 1;
	auto lut = Mode::getLogOpLUT(LOG);

	if ((ARG & MAJ) == 0) {
		// X-Axis is major direction.
		while (engineTime < limit) {
			engineTime += delta;
			Mode::psetColor(vram, ADX, DY, pitch, fgCol, WM, lut, LOG);

			ADX += TX;
			if (ASX < NY) {
				ASX += NX;
				DY += TY;
			}
			ASX -= NY;
			//ASX &= 1023; // mask to 10 bits range
			if (ANX++ == NX || (ADX & width)) {
				cmdReady(engineTime);
				break;
			}
		}
	} else {
		// Y-Axis is major direction.
		while (engineTime < limit) {
			engineTime += delta;
			Mode::psetColor(vram, ADX, DY, pitch, fgCol, WM, lut, LOG);
			DY += TY;
			if (ASX < NY) {
				ASX += NX;
				ADX += TX;
			}
			ASX -= NY;
			//ASX &= 1023; // mask to 10 bits range
			if (ANX++ == NX || (ADX & width)) {
				cmdReady(engineTime);
				break;
			}
		}
	}
}

// SRCH
void V9990CmdEngine::startSRCH(EmuTime::param time)
{
	engineTime = time;
	ASX = SX;
}

template<typename Mode>
void V9990CmdEngine::executeSRCH(EmuTime::param limit)
{
	using Type = typename Mode::Type;
	auto delta = getTiming(*this, SRCH_TIMING);
	unsigned width = vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);
	Type mask = (1 << Mode::BITS_PER_PIXEL) -1;

	word TX = (ARG & DIX) ? word(-1) : 1;
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
			col = narrow_cast<byte>((addr & 0x40000) ? (fgCol >> 8) : (fgCol & 0xFF));
			mask2 = Mode::shift(mask, 3, ASX);
		}
		if (((value & mask2) == (col & mask2)) ^ AEQ) {
			status |= BD; // border detected
			cmdReady(engineTime);
			borderX = ASX;
			break;
		}
		ASX += TX;
		if (ASX & width) {
			status &= ~BD; // border not detected
			cmdReady(engineTime);
			borderX = ASX;
			break;
		}
	}
}

// POINT
template<typename Mode>
void V9990CmdEngine::startPOINT(EmuTime::param /*time*/)
{
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	auto d = Mode::point(vram, SX, SY, pitch);

	if constexpr (Mode::BITS_PER_PIXEL != 16) {
		data = byte(d);
		endAfterRead = true;
	} else {
		unsigned tmp = d; // workaround for VC++ warning C4333
				  // (in case Mode::Type == byte and
				  //          Mode::BITS_PER_PIXEL == 8)
		data = narrow_cast<byte>(tmp & 0xff);
		partial = narrow_cast<byte>(tmp >> 8);
		endAfterRead = false;
	}
	status |= TR;
}

template<typename Mode>
void V9990CmdEngine::executePOINT(EmuTime::param /*limit*/)
{
	if (status & TR) return;

	assert(Mode::BITS_PER_PIXEL == 16);
	status |= TR;
	data = partial;
	endAfterRead = true;
}

// PSET
template<typename Mode>
void V9990CmdEngine::startPSET(EmuTime::param time)
{
	unsigned pitch = Mode::getPitch(vdp.getImageWidth());
	auto lut = Mode::getLogOpLUT(LOG);
	Mode::psetColor(vram, DX, DY, pitch, fgCol, WM, lut, LOG);

	// TODO advance DX DY

	cmdReady(time);
}

void V9990CmdEngine::executePSET(EmuTime::param /*limit*/)
{
	UNREACHABLE;
}

// ADVN
void V9990CmdEngine::startADVN(EmuTime::param time)
{
	std::cout << "V9990: ADVN not yet implemented\n";
	cmdReady(time); // TODO dummy implementation
}

void V9990CmdEngine::executeADVN(EmuTime::param /*limit*/)
{
	UNREACHABLE;
}

// ====================================================================
// CmdEngine methods

void V9990CmdEngine::sync2(EmuTime::param time)
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

void V9990CmdEngine::setCmdData(byte value, EmuTime::param time)
{
	sync(time);
	data = value;
	status &= ~TR;
}

byte V9990CmdEngine::getCmdData(EmuTime::param time)
{
	sync(time);

	byte value = 0xFF;
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

byte V9990CmdEngine::peekCmdData(EmuTime::param time) const
{
	const_cast<V9990CmdEngine*>(this)->sync(time);
	return (status & TR) ? data : 0xFF;
}

void V9990CmdEngine::cmdReady(EmuTime::param /*time*/)
{
	CMD = 0; // for deserialize
	status &= ~(CE | TR);
	vdp.cmdReady();
}

EmuTime V9990CmdEngine::estimateCmdEnd() const
{
	EmuDuration delta;
	switch (CMD >> 4) {
		case 0x00: // STOP
			delta = EmuDuration::zero();
			break;

		case 0x01: // LMMC
		case 0x05: // CMMC
			// command terminates when CPU writes data, no need for extra sync
			delta = EmuDuration::zero();
			break;

		case 0x03: // LMCM
		case 0x0D: // POINT
			// command terminates when CPU reads data, no need for extra sync
			delta = EmuDuration::zero();
			break;

		case 0x02: // LMMV
			// Block commands.
			delta = getTiming(*this, LMMV_TIMING) * (ANX + (ANY - 1) * getWrappedNX());
			break;
		case 0x04: // LMMM
			delta = getTiming(*this, LMMM_TIMING) * (ANX + (ANY - 1) * getWrappedNX());
			break;
		case 0x07: // CMMM
			delta = getTiming(*this, CMMM_TIMING) * (ANX + (ANY - 1) * getWrappedNX());
			break;
		case 0x08: // BMXL
			delta = getTiming(*this, BMXL_TIMING) * (ANX + (ANY - 1) * getWrappedNX()); // TODO correct?
			break;
		case 0x09: // BMLX
			delta = getTiming(*this, BMLX_TIMING) * (ANX + (ANY - 1) * getWrappedNX()); // TODO correct?
			break;

		case 0x06: // CMMK
			// Not yet implemented.
			delta = EmuDuration::zero();
			break;

		case 0x0A: // BMLL
			delta = getTiming(*this, BMLL_TIMING) * nbBytes;
			break;

		case 0x0B: // LINE
			delta = getTiming(*this, LINE_TIMING) * (NX - ANX); // TODO ignores clipping
			break;

		case 0x0C: // SRCH
			// Can end at any next pixel.
			delta = getTiming(*this, SRCH_TIMING);
			break;

		case 0x0E: // PSET
		case 0x0F: // ADVN
			// Current implementation of these commands execute instantly, no need for extra sync.
			delta = EmuDuration::zero();
			break;

		default: UNREACHABLE;
	}
	return engineTime + delta;
}

// version 1: initial version
// version 2: we forgot to serialize the time (or clock) member
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
	             "borderX",      borderX,
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

	if constexpr (Archive::IS_LOADER) {
		setCommandMode();
	}
}
INSTANTIATE_SERIALIZE_METHODS(V9990CmdEngine);

} // namespace openmsx
