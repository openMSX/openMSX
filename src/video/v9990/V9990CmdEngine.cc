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
#include "likely.hh"
#include "unreachable.hh"
#include <iostream>

namespace openmsx {

// 1st index  B0/2/4, B1/3/7, P1, P2
// 2nd index  sprites-ON, sprites-OFF, display-OFF
// 3th index  2bpp, 4bpp, 8bpp, 16bpp
//            (for P1/P2 fill in the same value 4 times)
const unsigned LMMV_TIMING[4][3][4] = {
	{ {  8, 11, 15, 30}, { 7, 10, 13, 26}, { 7, 10, 13, 25} },
	{ {  5,  7,  9, 18}, { 5,  6,  8, 17}, { 5,  6,  8, 17} },
	{ { 56, 56, 56, 56}, {25, 25, 25, 25}, { 9,  9,  9,  9} },
	{ { 28, 28, 28, 28}, {15, 15, 15, 15}, { 6,  6,  6,  6} }
};
const unsigned LMMM_TIMING[4][3][4] = {
	{ { 10, 16, 32, 66}, { 8, 14, 28, 57}, { 8, 13, 27, 54} },
	{ {  6, 10, 20, 39}, { 5,  9, 18, 35}, { 5,  9, 17, 35} },
	{ {115,115,115,115}, {52, 52, 52, 52}, {18, 18, 18, 18} },
	{ { 57, 57, 57, 57}, {25, 25, 25, 25}, { 9,  9,  9,  9} }
};
const unsigned BMXL_TIMING[4][3][4] = { // NOTE: values are BYTE based here!
	{ { 38, 33, 32, 33}, {33, 28, 28, 28}, {33, 27, 27, 27} }, // identical to LMMM (b)
	{ { 24, 20, 20, 19}, {22, 18, 18, 18}, {21, 17, 17, 17} }, // identical to LMMM (b)
	{ {171,171,171,171}, {82, 82, 82, 82}, {29, 29, 29, 29} },
	{ {114,114,114,114}, {50, 50, 50, 50}, {18, 18, 18, 18} }
};
const unsigned BMLX_TIMING[4][3][4] = {
	{ { 10, 16, 32, 66}, { 8, 14, 28, 57}, { 8, 13, 27, 54} }, // identical to LMMM
	{ {  6, 10, 20, 39}, { 5,  9, 18, 35}, { 5,  9, 17, 35} }, // identical to LMMM
	{ { 84, 84, 84, 84}, {44, 44, 44, 44}, {17, 17, 17, 17} },
	{ { 57, 57, 57, 57}, {25, 25, 25, 25}, { 9,  9,  9,  9} }
};
const unsigned BMLL_TIMING[4][3][4] = {
	{ { 33, 33, 33, 33}, {28, 28, 28, 28}, {27, 27, 27, 27} },
	{ { 20, 20, 20, 20}, {18, 18, 18, 18}, {18, 18, 18, 18} },
	{ {118,118,118,118}, {52, 52, 52, 52}, {18, 18, 18, 18} },
	{ {118,118,118,118}, {52, 52, 52, 52}, {18, 18, 18, 18} }
};
const unsigned CMMM_TIMING[4][3][4] = {
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }  // TODO
};
const unsigned LINE_TIMING[4][3][4] = {
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }  // TODO
};
const unsigned SRCH_TIMING[4][3][4] = {
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }, // TODO
	{ { 24, 24, 24, 24}, {24, 24, 24, 24}, {24, 24, 24, 24} }  // TODO
};


// Lazily initialized LUT to speed up logical operations:
//  - 1st index is the mode: 2,4,8 bpp or 'not-transparent'
//  - 2nd index is the logical operation: one of the 16 possible binary functions
// * Each entry contains a 256x256 byte array, that array is indexed using
//   destination and source byte (in that order).
// * A fully populated logOpLUT would take 4MB, however the vast majority of
//   this table is (almost) never used. So we save quite some memory (and
//   startup time) by lazily initializing this table.
static MemBuffer<byte> logOpLUT[4][16];
static byte bitLUT[8][16][2][2]; // to speedup calculating logOpLUT

enum { LOG_NO_T, LOG_BPP2, LOG_BPP4, LOG_BPP8 };
enum CommandMode { CMD_P1, CMD_P2, CMD_BPP2, CMD_BPP4, CMD_BPP8, CMD_BPP16 };

static void initBitTab()
{
	for (unsigned op = 0; op < 16; ++op) {
		unsigned tmp = op;
		for (unsigned src = 0; src < 2; ++src) {
			for (unsigned dst = 0; dst < 2; ++dst) {
				unsigned b = tmp & 1;
				for (unsigned bit = 0; bit < 8; ++bit) {
					bitLUT[bit][op][src][dst] = b << bit;
				}
				tmp >>= 1;
			}
		}
	}
}

static inline byte func01(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x03) == 0) return dst & 0x03;
	byte res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	return res;
}
static inline byte func23(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0C) == 0) return dst & 0x0C;
	byte res = 0;
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
static inline byte func45(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x30) == 0) return dst & 0x30;
	byte res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	return res;
}
static inline byte func67(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xC0) == 0) return dst & 0xC0;
	byte res = 0;
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

static inline byte func03(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0x0F) == 0) return dst & 0x0F;
	byte res = 0;
	res |= bitLUT[0][op][(src & 0x01) >> 0][(dst & 0x01) >> 0];
	res |= bitLUT[1][op][(src & 0x02) >> 1][(dst & 0x02) >> 1];
	res |= bitLUT[2][op][(src & 0x04) >> 2][(dst & 0x04) >> 2];
	res |= bitLUT[3][op][(src & 0x08) >> 3][(dst & 0x08) >> 3];
	return res;
}
static inline byte func47(unsigned op, unsigned src, unsigned dst)
{
	if ((src & 0xF0) == 0) return dst & 0xF0;
	byte res = 0;
	res |= bitLUT[4][op][(src & 0x10) >> 4][(dst & 0x10) >> 4];
	res |= bitLUT[5][op][(src & 0x20) >> 5][(dst & 0x20) >> 5];
	res |= bitLUT[6][op][(src & 0x40) >> 6][(dst & 0x40) >> 6];
	res |= bitLUT[7][op][(src & 0x80) >> 7][(dst & 0x80) >> 7];
	return res;
}

static inline byte func07(unsigned op, unsigned src, unsigned dst)
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

static void fillTableNoT(unsigned op, byte* table)
{
	for (unsigned dst = 0; dst < 256; ++dst) {
		for (unsigned src = 0; src < 256; ++src) {
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

static void fillTable2(unsigned op, byte* table)
{
	for (unsigned dst = 0; dst < 256; ++dst) {
		for (unsigned src = 0; src < 256; ++src) {
			byte res = 0;
			res |= func01(op, src, dst);
			res |= func23(op, src, dst);
			res |= func45(op, src, dst);
			res |= func67(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static void fillTable4(unsigned op, byte* table)
{
	for (unsigned dst = 0; dst < 256; ++dst) {
		for (unsigned src = 0; src < 256; ++src) {
			byte res = 0;
			res |= func03(op, src, dst);
			res |= func47(op, src, dst);
			table[dst * 256 + src] = res;
		}
	}
}

static void fillTable8(unsigned op, byte* table)
{
	for (unsigned dst = 0; dst < 256; ++dst) {
		{ // src == 0
			table[dst * 256 + 0  ] = dst;
		}
		for (unsigned src = 1; src < 256; ++src) { // src != 0
			table[dst * 256 + src] = func07(op, src, dst);
		}
	}
}

static const byte* getLogOpImpl(unsigned mode, unsigned op)
{
	op &= 0x0f;
	if (!logOpLUT[mode][op].data()) {
		logOpLUT[mode][op].resize(256 * 256);
		switch (mode) {
		case LOG_NO_T:
			fillTableNoT(op, logOpLUT[mode][op].data());
			break;
		case LOG_BPP2:
			fillTable2  (op, logOpLUT[mode][op].data());
			break;
		case LOG_BPP4:
			fillTable4  (op, logOpLUT[mode][op].data());
			break;
		case LOG_BPP8:
			fillTable8  (op, logOpLUT[mode][op].data());
			break;
		default:
			UNREACHABLE;
		}
	}
	return logOpLUT[mode][op].data();
}


static const byte DIY = 0x08;
static const byte DIX = 0x04;
static const byte NEQ = 0x02;
static const byte MAJ = 0x01;

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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990P1::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * ((toX & 1) - (fromX & 1));
	return (shift > 0) ? (value >> shift) : (value << -shift);
}

inline byte V9990CmdEngine::V9990P1::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline const byte* V9990CmdEngine::V9990P1::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? LOG_BPP4 : LOG_NO_T, op);
}

inline byte V9990CmdEngine::V9990P1::logOp(
	const byte* lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990P1::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}
inline void V9990CmdEngine::V9990P1::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = (addr & 0x40000) ? (color >> 8) : (color & 0xFF);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990P2::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * ((toX & 1) - (fromX & 1));
	return (shift > 0) ? (value >> shift) : (value << -shift);
}

inline byte V9990CmdEngine::V9990P2::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline const byte* V9990CmdEngine::V9990P2::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? LOG_BPP4 : LOG_NO_T, op);
}

inline byte V9990CmdEngine::V9990P2::logOp(
	const byte* lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990P2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990P2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = (addr & 0x40000) ? (color >> 8) : (color & 0xFF);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990Bpp2::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 2 * ((toX & 3) - (fromX & 3));
	return (shift > 0) ? (value >> shift) : (value << -shift);
}

inline byte V9990CmdEngine::V9990Bpp2::shiftMask(unsigned x)
{
	return 0xC0 >> (2 * (x & 3));
}

inline const byte* V9990CmdEngine::V9990Bpp2::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? LOG_BPP2 : LOG_NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp2::logOp(
	const byte* lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp2::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp2::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = (addr & 0x40000) ? (color >> 8) : (color & 0xFF);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	return vram.readVRAMDirect(addressOf(x, y, pitch));
}

inline byte V9990CmdEngine::V9990Bpp4::shift(
	byte value, unsigned fromX, unsigned toX)
{
	int shift = 4 * ((toX & 1) - (fromX & 1));
	return (shift > 0) ? (value >> shift) : (value << -shift);
}

inline byte V9990CmdEngine::V9990Bpp4::shiftMask(unsigned x)
{
	return (x & 1) ? 0x0F : 0xF0;
}

inline const byte* V9990CmdEngine::V9990Bpp4::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? LOG_BPP4 : LOG_NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp4::logOp(
	const byte* lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp4::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
	byte mask2 = mask1 & shiftMask(x);
	byte result = (dstColor & ~mask2) | (newColor & mask2);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp4::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = (addr & 0x40000) ? (color >> 8) : (color & 0xFF);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
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

inline const byte* V9990CmdEngine::V9990Bpp8::getLogOpLUT(byte op)
{
	return getLogOpImpl((op & 0x10) ? LOG_BPP8 : LOG_NO_T, op);
}

inline byte V9990CmdEngine::V9990Bpp8::logOp(
	const byte* lut, byte src, byte dst)
{
	return lut[256 * dst + src];
}

inline void V9990CmdEngine::V9990Bpp8::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	byte srcColor, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
	byte result = (dstColor & ~mask1) | (newColor & mask1);
	vram.writeVRAMDirect(addr, result);
}

inline void V9990CmdEngine::V9990Bpp8::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word color, word mask, const byte* lut, byte /*op*/)
{
	unsigned addr = addressOf(x, y, pitch);
	byte srcColor = (addr & 0x40000) ? (color >> 8) : (color & 0xFF);
	byte dstColor = vram.readVRAMDirect(addr);
	byte newColor = logOp(lut, srcColor, dstColor);
	byte mask1 = (addr & 0x40000) ? (mask >> 8) : (mask & 0xFF);
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
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch)
{
	unsigned addr = addressOf(x, y, pitch);
	return vram.readVRAMDirect(addr + 0x00000) +
	       vram.readVRAMDirect(addr + 0x40000) * 256;
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

inline const byte* V9990CmdEngine::V9990Bpp16::getLogOpLUT(byte op)
{
	return getLogOpImpl(LOG_NO_T, op);
}

inline word V9990CmdEngine::V9990Bpp16::logOp(
	const byte* lut, word src, word dst, bool transp)
{
	if (transp && (src == 0)) return dst;
	return (lut[((dst & 0x00FF) << 8) + ((src & 0x00FF) >> 0)] << 0) +
	       (lut[((dst & 0xFF00) << 0) + ((src & 0xFF00) >> 8)] << 8);
}

inline void V9990CmdEngine::V9990Bpp16::pset(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word srcColor, word mask, const byte* lut, byte op)
{
	unsigned addr = addressOf(x, y, pitch);
	word dstColor = vram.readVRAMDirect(addr + 0x00000) +
	                vram.readVRAMDirect(addr + 0x40000) * 256;
	word newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	word result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, result & 0xFF);
	vram.writeVRAMDirect(addr + 0x40000, result >> 8);
}

inline void V9990CmdEngine::V9990Bpp16::psetColor(
	V9990VRAM& vram, unsigned x, unsigned y, unsigned pitch,
	word srcColor, word mask, const byte* lut, byte op)
{
	unsigned addr = addressOf(x, y, pitch);
	word dstColor = vram.readVRAMDirect(addr + 0x00000) +
	                vram.readVRAMDirect(addr + 0x40000) * 256;
	word newColor = logOp(lut, srcColor, dstColor, (op & 0x10) != 0);
	word result = (dstColor & ~mask) | (newColor & mask);
	vram.writeVRAMDirect(addr + 0x00000, result & 0xFF);
	vram.writeVRAMDirect(addr + 0x40000, result >> 8);
}

// ====================================================================
/** Constructor
  */
V9990CmdEngine::V9990CmdEngine(V9990& vdp_, EmuTime::param time_,
                               RenderSettings& settings_)
	: settings(settings_), vdp(vdp_), time(time_)
{
	MSXMotherBoard::SharedStuff& info =
		vdp.getMotherBoard().getSharedStuff("v9990cmdtrace");
	if (info.counter == 0) {
		assert(info.stuff == nullptr);
		info.stuff = new BooleanSetting(
			vdp.getCommandController(),
			"v9990cmdtrace", "V9990 command tracing on/off", false);
	}
	++info.counter;
	cmdTraceSetting = reinterpret_cast<BooleanSetting*>(info.stuff);

	initBitTab();

	CmdSTOP* stopCmd = new CmdSTOP(*this, vdp.getVRAM());
	for (int mode = 0; mode < 6; ++mode) {
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

	update(settings.getCmdTiming());
	settings.getCmdTiming().attach(*this);

	reset(time);

	// avoid UMR on savestate
	srcAddress = dstAddress = nbBytes = 0;
	ASX = ADX = ANX = ANY = 0;
	SX = SY = DX = DY = NX = NY = 0;
	WM = fgCol = bgCol = 0;
	ARG = LOG = 0;
	data = bitsLeft = partial = 0;
}

template <template <class Mode> class Command>
void V9990CmdEngine::createEngines(int cmd)
{
	V9990VRAM& vram = vdp.getVRAM();
	commands[cmd][CMD_P1   ] = new Command<V9990P1>   (*this, vram);
	commands[cmd][CMD_P2   ] = new Command<V9990P2>   (*this, vram);
	commands[cmd][CMD_BPP2 ] = new Command<V9990Bpp2> (*this, vram);
	commands[cmd][CMD_BPP4 ] = new Command<V9990Bpp4> (*this, vram);
	commands[cmd][CMD_BPP8 ] = new Command<V9990Bpp8> (*this, vram);
	commands[cmd][CMD_BPP16] = new Command<V9990Bpp16>(*this, vram);
}

V9990CmdEngine::~V9990CmdEngine()
{
	settings.getCmdTiming().detach(*this);

	delete commands[0][0]; // Delete the STOP cmd

	for (int cmd = 1; cmd < 16; ++cmd) { // Delete the rest
		for (int mode = 0; mode < 6; ++mode) {
			delete commands[cmd][mode];
		}
	}

	MSXMotherBoard::SharedStuff& info =
		vdp.getMotherBoard().getSharedStuff("v9990cmdtrace");
	assert(info.counter);
	assert(cmdTraceSetting);
	assert(cmdTraceSetting == info.stuff);
	--info.counter;
	if (info.counter == 0) {
		delete cmdTraceSetting;
		info.stuff = nullptr;
	}
}

void V9990CmdEngine::reset(EmuTime::param /*time*/)
{
	currentCommand = nullptr;
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
	case 20: { // CMD
		CMD = value;
		if (cmdTraceSetting->getValue()) {
			reportV9990Command();
		}
		status |= CE;

		setCurrentCommand();
		currentCommand->start(time);

		// Finish command now if instantaneous command timing is active.
		// Some commands are already instantaneous, so check for nullptr
		// is needed.
		if (brokenTiming && currentCommand) {
			currentCommand->execute(time);
		}
		break;
	}
	}
}

void V9990CmdEngine::setCurrentCommand()
{
	CommandMode cmdMode;
	V9990DisplayMode dispMode = vdp.getDisplayMode();
	if (dispMode == P1) {
		cmdMode = CMD_P1;
	} else if (dispMode == P2) {
		cmdMode = CMD_P2;
	} else { // Bx
		switch (vdp.getColorMode()) {
			default:
				UNREACHABLE;
			case BP2:
				cmdMode = CMD_BPP2;
				break;
			case PP:
			case BP4:
				cmdMode = CMD_BPP4;
				break;
			case BYUV:
			case BYUVP:
			case BYJK:
			case BYJKP:
			case BD8:
			case BP6:
				cmdMode = CMD_BPP8;
				break;
			case BD16:
				cmdMode = CMD_BPP16;
				break;
		}
	}
	currentCommand = commands[CMD >> 4][cmdMode];
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
	          << " ARG=" << std::hex << int(ARG)
	          << " LOG=" << std::hex << int(LOG)
	          << " WM="  << std::hex << WM
	          << " FC="  << std::hex << fgCol
	          << " BC="  << std::hex << bgCol
	          << " CMD=" << std::hex << int(CMD)
	          << std::endl;
}

void V9990CmdEngine::update(const Setting& setting)
{
	brokenTiming = static_cast<const EnumSetting<bool>&>(setting).getValue();
}

EmuDuration V9990CmdEngine::getTiming(const unsigned table[4][3][4]) const
{
	if (unlikely(brokenTiming)) return EmuDuration();

	V9990DisplayMode mode = vdp.getDisplayMode();
	unsigned idx1 = (mode == P1) ? 2 :
	                (mode == P2) ? 3 :
	                (vdp.isOverScan()) ? 0 : 1;
	unsigned idx2 = vdp.isDisplayEnabled() ? (vdp.spritesEnabled() ? 0 : 1)
	                                       : 2;
	unsigned idx3 = vdp.getColorDepth();
	// TODO possible optimization: directly put EmuDurations in table
	unsigned x = table[idx1][idx2][idx3];
	return Clock<V9990DisplayTiming::UC_TICKS_PER_SECOND>::duration(x);
}

// ====================================================================
// V9990Cmd

V9990CmdEngine::V9990Cmd::V9990Cmd(V9990CmdEngine& engine_,
                                   V9990VRAM& vram_)
	: engine(engine_), vram(vram_)
{
}

V9990CmdEngine::V9990Cmd::~V9990Cmd()
{
}

// ====================================================================
// STOP

V9990CmdEngine::CmdSTOP::CmdSTOP(V9990CmdEngine& engine_,
                                 V9990VRAM& vram_)
	: V9990Cmd(engine_, vram_)
{
}

void V9990CmdEngine::CmdSTOP::start(EmuTime::param time)
{
	engine.cmdReady(time);
}

void V9990CmdEngine::CmdSTOP::execute(EmuTime::param /*time*/)
{
}

// ====================================================================
// LMMC

template <class Mode>
V9990CmdEngine::CmdLMMC<Mode>::CmdLMMC(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMC<Mode>::start(EmuTime::param /*time*/)
{
	if (Mode::BITS_PER_PIXEL == 16) {
		engine.bitsLeft = 1;
	}
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
	engine.status |= TR;
}

template <>
void V9990CmdEngine::CmdLMMC<V9990CmdEngine::V9990Bpp16>::execute(
	EmuTime::param time)
{
	if (!(engine.status & TR)) {
		engine.status |= TR;
		if (engine.bitsLeft) {
			engine.bitsLeft = 0;
			engine.partial = engine.data;
		} else {
			engine.bitsLeft = 1;
			word value = (engine.data << 8) | engine.partial;
			unsigned pitch = V9990Bpp16::getPitch(engine.vdp.getImageWidth());
			const byte* lut = V9990Bpp16::getLogOpLUT(engine.LOG);
			V9990Bpp16::pset(vram, engine.DX, engine.DY, pitch,
			           value, engine.WM, lut, engine.LOG);
			int dx = (engine.ARG & DIX) ? -1 : 1;
			engine.DX += dx;
			if (!--(engine.ANX)) {
				int dy = (engine.ARG & DIY) ? -1 : 1;
				engine.DX -= (engine.NX * dx);
				engine.DY += dy;
				if (!--(engine.ANY)) {
					engine.cmdReady(time);
				} else {
					engine.ANX = engine.getWrappedNX();
				}
			}
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdLMMC<Mode>::execute(EmuTime::param time)
{
	if (!(engine.status & TR)) {
		engine.status |= TR;
		unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
		const byte* lut = Mode::getLogOpLUT(engine.LOG);
		for (int i = 0; (engine.ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			byte data = Mode::shift(engine.data, i, engine.DX);
			Mode::pset(vram, engine.DX, engine.DY, pitch,
			           data, engine.WM, lut, engine.LOG);

			int dx = (engine.ARG & DIX) ? -1 : 1;
			engine.DX += dx;
			if (!--(engine.ANX)) {
				int dy = (engine.ARG & DIY) ? -1 : 1;
				engine.DX -= (engine.NX * dx);
				engine.DY += dy;
				if (!--(engine.ANY)) {
					engine.cmdReady(time);
				} else {
					engine.ANX = engine.NX;
				}
			}
		}
	}
}

// ====================================================================
// LMMV

template <class Mode>
V9990CmdEngine::CmdLMMV<Mode>::CmdLMMV(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
}

template <class Mode>
void V9990CmdEngine::CmdLMMV<Mode>::execute(EmuTime::param time)
{
	// TODO can be optimized a lot

	auto delta = engine.getTiming(LMMV_TIMING);
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = Mode::getLogOpLUT(engine.LOG);
	while (engine.time < time) {
		engine.time += delta;
		Mode::psetColor(vram, engine.DX, engine.DY, pitch,
		                engine.fgCol, engine.WM, lut, engine.LOG);

		engine.DX += dx;
		if (!--(engine.ANX)) {
			engine.DX -= (engine.NX * dx);
			engine.DY += dy;
			if (!--(engine.ANY)) {
				engine.cmdReady(engine.time);
				return;
			} else {
				engine.ANX = engine.getWrappedNX();
			}
		}
	}
}

// ====================================================================
// LMCM

template <class Mode>
V9990CmdEngine::CmdLMCM<Mode>::CmdLMCM(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMCM<Mode>::start(EmuTime::param /*time*/)
{
	if (Mode::BITS_PER_PIXEL == 16) {
		engine.bitsLeft = 0;
	}
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
	engine.status &= ~TR;
	engine.endAfterRead = false;
}

template <class Mode>
void V9990CmdEngine::CmdLMCM<Mode>::execute(EmuTime::param /*time*/)
{
	if (!(engine.status & TR)) {
		engine.status |= TR;
		if ((Mode::BITS_PER_PIXEL == 16) && engine.bitsLeft) {
			engine.bitsLeft = 0;
			engine.data = engine.partial;
			return;
		}
		unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
		typename Mode::Type data = 0;
		for (int i = 0; (engine.ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			typename Mode::Type src = Mode::point(vram, engine.SX, engine.SY, pitch);
			data |= Mode::shift(src, engine.SX, i) & Mode::shiftMask(i);

			int dx = (engine.ARG & DIX) ? -1 : 1;
			engine.SX += dx;
			if (!--(engine.ANX)) {
				int dy = (engine.ARG & DIY) ? -1 : 1;
				engine.SX -= (engine.NX * dx);
				engine.SY += dy;
				if (!--(engine.ANY)) {
					engine.endAfterRead = true;
				} else {
					engine.ANX = engine.getWrappedNX();
				}
			}
		}
		if (Mode::BITS_PER_PIXEL == 16) {
			unsigned tmp = data;	// workaround for VC++ warning C4333
									// (in case Mode::Type == byte and
									//          Mode::BITS_PER_PIXEL == 8)
			engine.data = tmp & 0xff;
			engine.partial = tmp >> 8;
			engine.bitsLeft = 1;
		} else {
			engine.data = byte(data);
		}
	}
}

// ====================================================================
// LMMM

template <class Mode>
V9990CmdEngine::CmdLMMM<Mode>::CmdLMMM(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
}

template <class Mode>
void V9990CmdEngine::CmdLMMM<Mode>::execute(EmuTime::param time)
{
	// TODO can be optimized a lot

	auto delta = engine.getTiming(LMMM_TIMING);
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = Mode::getLogOpLUT(engine.LOG);
	while (engine.time < time) {
		engine.time += delta;
		typename Mode::Type src = Mode::point(vram, engine.SX, engine.SY, pitch);
		src = Mode::shift(src, engine.SX, engine.DX);
		Mode::pset(vram, engine.DX, engine.DY, pitch,
		           src, engine.WM, lut, engine.LOG);

		engine.DX += dx;
		engine.SX += dx;
		if (!--(engine.ANX)) {
			engine.DX -= (engine.NX * dx);
			engine.SX -= (engine.NX * dx);
			engine.DY += dy;
			engine.SY += dy;
			if (!--(engine.ANY)) {
				engine.cmdReady(engine.time);
				return;
			} else {
				engine.ANX = engine.getWrappedNX();
			}
		}
	}
}

// ====================================================================
// CMMC

template <class Mode>
V9990CmdEngine::CmdCMMC<Mode>::CmdCMMC(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMC<Mode>::start(EmuTime::param /*time*/)
{
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
	engine.status |= TR;
}

template <class Mode>
void V9990CmdEngine::CmdCMMC<Mode>::execute(EmuTime::param time)
{
	if (!(engine.status & TR)) {
		engine.status |= TR;

		unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
		int dx = (engine.ARG & DIX) ? -1 : 1;
		int dy = (engine.ARG & DIY) ? -1 : 1;
		const byte* lut = Mode::getLogOpLUT(engine.LOG);
		for (unsigned i = 0; i < 8; ++i) {
			bool bit = (engine.data & 0x80) != 0;
			engine.data <<= 1;

			word src = bit ? engine.fgCol : engine.bgCol;
			Mode::psetColor(vram, engine.DX, engine.DY, pitch,
			                src, engine.WM, lut, engine.LOG);

			engine.DX += dx;
			if (!--(engine.ANX)) {
				engine.DX -= (engine.NX * dx);
				engine.DY += dy;
				if (!--(engine.ANY)) {
					engine.cmdReady(time);
					return;
				} else {
					engine.ANX = engine.getWrappedNX();
				}
			}
		}
	}
}

// ====================================================================
// CMMK

template <class Mode>
V9990CmdEngine::CmdCMMK<Mode>::CmdCMMK(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMK<Mode>::start(EmuTime::param time)
{
	std::cout << "V9990: CMMK not yet implemented" << std::endl;
	engine.cmdReady(time); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdCMMK<Mode>::execute(EmuTime::param /*time*/)
{
}

// ====================================================================
// CMMM

template <class Mode>
V9990CmdEngine::CmdCMMM<Mode>::CmdCMMM(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.srcAddress = (engine.SX & 0xFF) + ((engine.SY & 0x7FF) << 8);
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
	engine.bitsLeft = 0;
}

template <class Mode>
void V9990CmdEngine::CmdCMMM<Mode>::execute(EmuTime::param time)
{
	// TODO can be optimized a lot

	auto delta = engine.getTiming(CMMM_TIMING);
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = Mode::getLogOpLUT(engine.LOG);
	while (engine.time < time) {
		engine.time += delta;
		if (!engine.bitsLeft) {
			engine.data = vram.readVRAMBx(engine.srcAddress++);
			engine.bitsLeft = 8;
		}
		--engine.bitsLeft;
		bool bit = (engine.data & 0x80) != 0;
		engine.data <<= 1;

		word color = bit ? engine.fgCol : engine.bgCol;
		Mode::psetColor(vram, engine.DX, engine.DY, pitch,
		                color, engine.WM, lut, engine.LOG);

		engine.DX += dx;
		if (!--(engine.ANX)) {
			engine.DX -= (engine.NX * dx);
			engine.DY += dy;
			if (!--(engine.ANY)) {
				engine.cmdReady(engine.time);
				return;
			} else {
				engine.ANX = engine.getWrappedNX();
			}
		}
	}
}

// ====================================================================
// BMXL

template <class Mode>
V9990CmdEngine::CmdBMXL<Mode>::CmdBMXL(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.srcAddress = (engine.SX & 0xFF) + ((engine.SY & 0x7FF) << 8);
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
}

template <>
void V9990CmdEngine::CmdBMXL<V9990CmdEngine::V9990Bpp16>::execute(
	EmuTime::param time)
{
	// timing value is times 2, because it does 2 bytes per iteration:
	auto delta = engine.getTiming(BMXL_TIMING) * 2;
	unsigned pitch = V9990Bpp16::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = V9990Bpp16::getLogOpLUT(engine.LOG);

	while (engine.time < time) {
		engine.time += delta;
		word src = vram.readVRAMBx(engine.srcAddress + 0) +
		           vram.readVRAMBx(engine.srcAddress + 1) * 256;
		engine.srcAddress += 2;
		V9990Bpp16::pset(vram, engine.DX, engine.DY, pitch,
		                 src, engine.WM, lut, engine.LOG);
		engine.DX += dx;
		if (!--(engine.ANX)) {
			engine.DX -= (engine.NX * dx);
			engine.DY += dy;
			if (!--(engine.ANY)) {
				engine.cmdReady(engine.time);
				return;
			} else {
				engine.ANX = engine.getWrappedNX();
			}
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdBMXL<Mode>::execute(EmuTime::param time)
{
	auto delta = engine.getTiming(BMXL_TIMING);
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = Mode::getLogOpLUT(engine.LOG);

	while (engine.time < time) {
		engine.time += delta;
		byte data = vram.readVRAMBx(engine.srcAddress++);
		for (int i = 0; (engine.ANY > 0) && (i < Mode::PIXELS_PER_BYTE); ++i) {
			Mode::pset(vram, engine.DX, engine.DY, pitch,
			           data, engine.WM, lut, engine.LOG);
			engine.DX += dx;
			if (!--(engine.ANX)) {
				engine.DX -= (engine.NX * dx);
				engine.DY += dy;
				if (!--(engine.ANY)) {
					engine.cmdReady(engine.time);
					return;
				} else {
					engine.ANX = engine.getWrappedNX();
				}
			}
		}
	}
}

// ====================================================================
// BMLX

template <class Mode>
V9990CmdEngine::CmdBMLX<Mode>::CmdBMLX(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.dstAddress = (engine.DX & 0xFF) + ((engine.DY & 0x7FF) << 8);
	engine.ANX = engine.getWrappedNX();
	engine.ANY = engine.getWrappedNY();
}

template <class Mode>
void V9990CmdEngine::CmdBMLX<Mode>::execute(EmuTime::param time)
{
	// TODO lots of corner cases still go wrong
	//      very dumb implementation, can be made much faster
	auto delta = engine.getTiming(BMLX_TIMING);
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	int dx = (engine.ARG & DIX) ? -1 : 1;
	int dy = (engine.ARG & DIY) ? -1 : 1;

	word tmp = 0;
	engine.bitsLeft = 16;
	while (engine.time < time) {
		engine.time += delta;
		typename Mode::Type src = Mode::point(vram, engine.SX, engine.SY, pitch);
		src = Mode::shift(src, engine.SX, 0); // TODO optimize
		if (Mode::BITS_PER_PIXEL == 16) {
			tmp = src;
		} else {
			tmp <<= Mode::BITS_PER_PIXEL;
			tmp |= src;
		}
		engine.bitsLeft -= Mode::BITS_PER_PIXEL;
		if (!engine.bitsLeft) {
			vram.writeVRAMBx(engine.dstAddress++, tmp & 0xFF);
			vram.writeVRAMBx(engine.dstAddress++, tmp >> 8);
			engine.bitsLeft = 16;
			tmp = 0;
		}

		engine.DX += dx;
		engine.SX += dx;
		if (!--(engine.ANX)) {
			engine.DX -= (engine.NX * dx);
			engine.SX -= (engine.NX * dx);
			engine.DY += dy;
			engine.SY += dy;
			if (!--(engine.ANY)) {
				engine.cmdReady(engine.time);
				// TODO handle last pixels
				return;
			} else {
				engine.ANX = engine.getWrappedNX();
			}
		}
	}
}

// ====================================================================
// BMLL

template <class Mode>
V9990CmdEngine::CmdBMLL<Mode>::CmdBMLL(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdBMLL<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.srcAddress = (engine.SX & 0xFF) + ((engine.SY & 0x7FF) << 8);
	engine.dstAddress = (engine.DX & 0xFF) + ((engine.DY & 0x7FF) << 8);
	engine.nbBytes    = (engine.NX & 0xFF) + ((engine.NY & 0x7FF) << 8);
	if (engine.nbBytes == 0) {
		engine.nbBytes = 0x80000;
	}
	if (Mode::BITS_PER_PIXEL == 16) {
		// TODO is this correct???
		// drop last bit
		engine.srcAddress >>= 1;
		engine.dstAddress >>= 1;
		engine.nbBytes    >>= 1;
	}
}

template <>
void V9990CmdEngine::CmdBMLL<V9990CmdEngine::V9990Bpp16>::execute(EmuTime::param time)
{
	// TODO DIX DIY?
	// timing value is times 2, because it does 2 bytes per iteration:
	auto delta = engine.getTiming(BMLL_TIMING) * 2;
	const byte* lut = V9990Bpp16::getLogOpLUT(engine.LOG);
	bool transp = (engine.LOG & 0x10) != 0;
	while (engine.time < time) {
		engine.time += delta;
		// VRAM always mapped as in Bx modes
		word srcColor = vram.readVRAMDirect(engine.srcAddress + 0x00000) +
		                vram.readVRAMDirect(engine.srcAddress + 0x40000) * 256;
		word dstColor = vram.readVRAMDirect(engine.dstAddress + 0x00000) +
		                vram.readVRAMDirect(engine.dstAddress + 0x40000) * 256;
		word newColor = V9990Bpp16::logOp(lut, srcColor, dstColor, transp);
		word result = (dstColor & ~engine.WM) | (newColor & engine.WM);
		vram.writeVRAMDirect(engine.dstAddress + 0x00000, result & 0xFF);
		vram.writeVRAMDirect(engine.dstAddress + 0x40000, result >> 8);
		engine.srcAddress = (engine.srcAddress + 1) & 0x3FFFF;
		engine.dstAddress = (engine.dstAddress + 1) & 0x3FFFF;
		if (!--engine.nbBytes) {
			engine.cmdReady(engine.time);
			return;
		}
	}
}

template <class Mode>
void V9990CmdEngine::CmdBMLL<Mode>::execute(EmuTime::param time)
{
	// TODO DIX DIY?
	auto delta = engine.getTiming(BMLL_TIMING);
	const byte* lut = Mode::getLogOpLUT(engine.LOG);
	while (engine.time < time) {
		engine.time += delta;
		// VRAM always mapped as in Bx modes
		byte srcColor = vram.readVRAMBx(engine.srcAddress);
		unsigned addr = V9990VRAM::transformBx(engine.dstAddress);
		byte dstColor = vram.readVRAMDirect(addr);
		byte newColor = Mode::logOp(lut, srcColor, dstColor);
		byte mask = (addr & 0x40000) ? (engine.WM >> 8) : (engine.WM & 0xFF);
		byte result = (dstColor & ~mask) | (newColor & mask);
		vram.writeVRAMDirect(addr, result);
		engine.srcAddress = (engine.srcAddress + 1) & 0x7FFFF;
		engine.dstAddress = (engine.dstAddress + 1) & 0x7FFFF;
		if (!--engine.nbBytes) {
			engine.cmdReady(engine.time);
			return;
		}
	}
}

// ====================================================================
// LINE

template <class Mode>
V9990CmdEngine::CmdLINE<Mode>::CmdLINE(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdLINE<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.ASX = (engine.NX - 1) / 2;
	engine.ADX = engine.DX;
	engine.ANX = 0;
}

template <class Mode>
void V9990CmdEngine::CmdLINE<Mode>::execute(EmuTime::param time)
{
	auto delta = engine.getTiming(LINE_TIMING);
	unsigned width = engine.vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);

	int TX = (engine.ARG & DIX) ? -1 : 1;
	int TY = (engine.ARG & DIY) ? -1 : 1;
	const byte* lut = Mode::getLogOpLUT(engine.LOG);

	if ((engine.ARG & MAJ) == 0) {
		// X-Axis is major direction.
		while (engine.time < time) {
			engine.time += delta;
			Mode::psetColor(vram, engine.ADX, engine.DY, pitch,
			                engine.fgCol, engine.WM, lut, engine.LOG);

			engine.ADX += TX;
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.DY += TY;
			}
			engine.ASX -= engine.NY;
			//engine.ASX &= 1023; // mask to 10 bits range
			if (engine.ANX++ == engine.NX || (engine.ADX & width)) {
				engine.cmdReady(engine.time);
				break;
			}
		}
	} else {
		// Y-Axis is major direction.
		while (engine.time < time) {
			engine.time += delta;
			Mode::psetColor(vram, engine.ADX, engine.DY, pitch,
			                engine.fgCol, engine.WM, lut, engine.LOG);
			engine.DY += TY;
			if (engine.ASX < engine.NY) {
				engine.ASX += engine.NX;
				engine.ADX += TX;
			}
			engine.ASX -= engine.NY;
			//engine.ASX &= 1023; // mask to 10 bits range
			if (engine.ANX++ == engine.NX || (engine.ADX & width)) {
				engine.cmdReady(engine.time);
				break;
			}
		}
	}
}

// ====================================================================
// SRCH

template <class Mode>
V9990CmdEngine::CmdSRCH<Mode>::CmdSRCH(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdSRCH<Mode>::start(EmuTime::param time)
{
	engine.time = time;
	engine.ASX = engine.SX;
}

template <class Mode>
void V9990CmdEngine::CmdSRCH<Mode>::execute(EmuTime::param time)
{
	auto delta = engine.getTiming(SRCH_TIMING);
	unsigned width = engine.vdp.getImageWidth();
	unsigned pitch = Mode::getPitch(width);
	typename Mode::Type mask = (1 << Mode::BITS_PER_PIXEL) -1;

	int TX = (engine.ARG & DIX) ? -1 : 1;
	bool AEQ = (engine.ARG & NEQ) != 0;

	while (engine.time < time) {
		engine.time += delta;
		typename Mode::Type value;
		typename Mode::Type col;
		typename Mode::Type mask2;
		if (Mode::BITS_PER_PIXEL == 16) {
			value = Mode::point(vram, engine.ASX, engine.SY, pitch);
			col = static_cast<typename Mode::Type>(engine.fgCol);
			mask2 = static_cast<typename Mode::Type>(~0);
		} else {
			// TODO check
			unsigned addr = Mode::addressOf(engine.ASX, engine.SY, pitch);
			value = vram.readVRAMDirect(addr);
			col = (addr & 0x40000) ? (engine.fgCol >> 8) : (engine.fgCol & 0xFF);
			mask2 = Mode::shift(mask, 3, engine.ASX);
		}
		if (((value & mask2) == (col & mask2)) ^ AEQ) {
			engine.status |= BD; // border detected
			engine.cmdReady(engine.time);
			engine.borderX = engine.ASX;
			break;
		}
		if ((engine.ASX += TX) & width) {
			engine.status &= ~BD; // border not detected
			engine.cmdReady(engine.time);
			engine.borderX = engine.ASX;
			break;
		}
	}
}

// ====================================================================
// POINT

template <class Mode>
V9990CmdEngine::CmdPOINT<Mode>::CmdPOINT(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdPOINT<Mode>::start(EmuTime::param time)
{
	std::cout << "V9990: POINT not yet implemented" << std::endl;
	engine.cmdReady(time); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdPOINT<Mode>::execute(EmuTime::param /*time*/)
{
}

// ====================================================================
// PSET

template <class Mode>
V9990CmdEngine::CmdPSET<Mode>::CmdPSET(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdPSET<Mode>::start(EmuTime::param time)
{
	unsigned pitch = Mode::getPitch(engine.vdp.getImageWidth());
	const byte* lut = Mode::getLogOpLUT(engine.LOG);
	Mode::psetColor(vram, engine.DX, engine.DY, pitch,
	                engine.fgCol, engine.WM, lut, engine.LOG);

	// TODO advance DX DY

	engine.cmdReady(time);
}

template <class Mode>
void V9990CmdEngine::CmdPSET<Mode>::execute(EmuTime::param /*time*/)
{
}

// ====================================================================
// ADVN

template <class Mode>
V9990CmdEngine::CmdADVN<Mode>::CmdADVN(V9990CmdEngine& engine,
                                       V9990VRAM& vram)
	: V9990Cmd(engine, vram)
{
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::start(EmuTime::param time)
{
	std::cout << "V9990: ADVN not yet implemented" << std::endl;
	engine.cmdReady(time); // TODO dummy implementation
}

template <class Mode>
void V9990CmdEngine::CmdADVN<Mode>::execute(EmuTime::param /*time*/)
{
}

// ====================================================================
// CmdEngine methods

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

byte V9990CmdEngine::peekCmdData(EmuTime::param time)
{
	sync(time);
	return (status & TR) ? data : 0xFF;
}

void V9990CmdEngine::cmdReady(EmuTime::param /*time*/)
{
	currentCommand = nullptr;
	CMD = 0; // for deserialize
	status &= ~(CE | TR);
	vdp.cmdReady();
}

// version 1: initial version
// version 2: we forgot to serialize the time (or clock) member
template<typename Archive>
void V9990CmdEngine::serialize(Archive& ar, unsigned version)
{
	// note: V9990Cmd objects are stateless
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("time", time);
	} else {
		// In version 1 we forgot to serialize the time member (it was
		// also still a Clock back then). The best we can do is
		// initialize it with the current time, that's already done in
		// the constructor.
	}
	ar.serialize("srcAddress", srcAddress);
	ar.serialize("dstAddress", dstAddress);
	ar.serialize("nbBytes", nbBytes);
	ar.serialize("borderX", borderX);
	ar.serialize("ASX", ASX);
	ar.serialize("ADX", ADX);
	ar.serialize("ANX", ANX);
	ar.serialize("ANY", ANY);
	ar.serialize("SX", SX);
	ar.serialize("SY", SY);
	ar.serialize("DX", DX);
	ar.serialize("DY", DY);
	ar.serialize("NX", NX);
	ar.serialize("NY", NY);
	ar.serialize("WM", WM);
	ar.serialize("fgCol", fgCol);
	ar.serialize("bgCol", bgCol);
	ar.serialize("ARG", ARG);
	ar.serialize("LOG", LOG);
	ar.serialize("CMD", CMD);
	ar.serialize("status", status);
	ar.serialize("data", data);
	ar.serialize("bitsLeft", bitsLeft);
	ar.serialize("partial", partial);
	ar.serialize("endAfterRead", endAfterRead);

	if (ar.isLoader()) {
		if (CMD >> 4) {
			setCurrentCommand();
		} else {
			currentCommand = nullptr;
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(V9990CmdEngine);

} // namespace openmsx
