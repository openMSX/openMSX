// Code based on DOSBox-0.65

#include "ZMBVEncoder.hh"
#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "cstd.hh"
#include "endian.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <tuple>

namespace openmsx {

constexpr uint8_t DBZV_VERSION_HIGH = 0;
constexpr uint8_t DBZV_VERSION_LOW = 1;
constexpr uint8_t COMPRESSION_ZLIB = 1;
constexpr unsigned MAX_VECTOR = 16;
constexpr unsigned BLOCK_WIDTH  = MAX_VECTOR;
constexpr unsigned BLOCK_HEIGHT = MAX_VECTOR;
constexpr unsigned FLAG_KEYFRAME = 0x01;

struct CodecVector {
	int8_t x;
	int8_t y;
};

constexpr unsigned VECTOR_TAB_SIZE =
	1 +                                       // center
	8 * MAX_VECTOR +                          // horizontal, vertical, diagonal
	MAX_VECTOR * MAX_VECTOR - 2 * MAX_VECTOR; // rest (only MAX_VECTOR/2)

constexpr auto vectorTable = [] {
	std::array<CodecVector, VECTOR_TAB_SIZE> result = {};

	unsigned p = 0;
	// center
	result[p] = {0, 0};
	p += 1;
	// horizontal, vertical, diagonal
	for (int i = 1; i <= int(MAX_VECTOR); ++i, p += 8) {
		result[p + 0] = {int8_t( i), int8_t( 0)};
		result[p + 1] = {int8_t(-i), int8_t( 0)};
		result[p + 2] = {int8_t( 0), int8_t( i)};
		result[p + 3] = {int8_t( 0), int8_t(-i)};
		result[p + 4] = {int8_t( i), int8_t( i)};
		result[p + 5] = {int8_t(-i), int8_t( i)};
		result[p + 6] = {int8_t( i), int8_t(-i)};
		result[p + 7] = {int8_t(-i), int8_t(-i)};
	}
	// rest
	for (int y = 1; y <= int(MAX_VECTOR / 2); ++y) {
		for (int x = 1; x <= int(MAX_VECTOR / 2); ++x) {
			if (x == y) continue; // already have diagonal
			result[p + 0] = {int8_t( x), int8_t( y)};
			result[p + 1] = {int8_t(-x), int8_t( y)};
			result[p + 2] = {int8_t( x), int8_t(-y)};
			result[p + 3] = {int8_t(-x), int8_t(-y)};
			p += 4;
		}
	}
	assert(p == VECTOR_TAB_SIZE);

	// sort
	auto compare = [](const CodecVector& l, const CodecVector& r) {
		auto cost = [](const CodecVector& v) {
			auto c = cstd::sqrt(double(v.x * v.x + v.y * v.y));
			if ((v.x == 0) || (v.y == 0)) {
				// no penalty for purely horizontal/vertical offset
				c *= 1.0;
			} else if (cstd::abs(v.x) == cstd::abs(v.y)) {
				// small penalty for pure diagonal
				c *= 2.0;
			} else {
				// bigger penalty for 'random' direction
				c *= 4.0;
			}
			return c;
		};
		return std::tuple(cost(l), l.x, l.y) <
		       std::tuple(cost(r), r.x, r.y);
	};
	ranges::sort(result, compare);

	return result;
}();

struct KeyframeHeader {
	uint8_t high_version;
	uint8_t low_version;
	uint8_t compression;
	uint8_t format;
	uint8_t blockwidth;
	uint8_t blockheight;
};


static inline void writePixel(
	const PixelOperations<uint16_t>& pixelOps,
	uint16_t pixel, Endian::L16& dest)
{
	unsigned r = pixelOps.red256(pixel);
	unsigned g = pixelOps.green256(pixel);
	unsigned b = pixelOps.blue256(pixel);
	dest = ((r & 0xF8) << (11 - 3)) | ((g & 0xFC) << (5 - 2)) | (b >> 3);
}

static inline void writePixel(
	const PixelOperations<unsigned>& pixelOps,
	unsigned pixel, Endian::L32& dest)
{
	unsigned r = pixelOps.red256(pixel);
	unsigned g = pixelOps.green256(pixel);
	unsigned b = pixelOps.blue256(pixel);
	dest = (r << 16) | (g <<  8) |  b;
}


ZMBVEncoder::ZMBVEncoder(unsigned width_, unsigned height_, unsigned bpp)
	: width(width_)
	, height(height_)
{
	setupBuffers(bpp);
	memset(&zstream, 0, sizeof(zstream));
	deflateInit(&zstream, 6); // compression level

	// I did a small test: compression level vs compression speed
	//  (recorded Space Manbow intro, video only)
	//
	// level |  time  | size
	// ------+--------+----------
	//   0   | 1m12.6 | 139442594
	//   1   | 1m12.1 |   5217288
	//   2   | 1m10.8 |   4887258
	//   3   | 1m11.8 |   4610668
	//   4   | 1m13.1 |   3791932  <-- old default
	//   5   | 1m14.2 |   3602078
	//   6   | 1m14.5 |   3363766  <-- current default
	//   7   | 1m15.8 |   3333938
	//   8   | 1m25.0 |   3301168
	//   9   | 2m04.1 |   3253706
	//
	// Level 6 seems a good compromise between size/speed for THIS test.
}

void ZMBVEncoder::setupBuffers(unsigned bpp)
{
	switch (bpp) {
#if HAVE_16BPP
	case 15:
	case 16:
		format = ZMBV_FORMAT_16BPP;
		pixelSize = 2;
		break;
#endif
#if HAVE_32BPP
	case 32:
		format = ZMBV_FORMAT_32BPP;
		pixelSize = 4;
		break;
#endif
	default:
		UNREACHABLE;
	}

	pitch = width + 2 * MAX_VECTOR;
	unsigned bufsize = (height + 2 * MAX_VECTOR) * pitch * pixelSize + 2048;

	oldframe.resize(bufsize);
	newframe.resize(bufsize);
	memset(oldframe.data(), 0, bufsize);
	memset(newframe.data(), 0, bufsize);
	work.resize(bufsize);
	outputSize = neededSize();
	output.resize(outputSize);

	assert((width  % BLOCK_WIDTH ) == 0);
	assert((height % BLOCK_HEIGHT) == 0);
	unsigned xBlocks = width / BLOCK_WIDTH;
	unsigned yBlocks = height / BLOCK_HEIGHT;
	blockOffsets.resize(xBlocks * yBlocks);
	for (auto y : xrange(yBlocks)) {
		for (auto x : xrange(xBlocks)) {
			blockOffsets[y * xBlocks + x] =
				((y * BLOCK_HEIGHT) + MAX_VECTOR) * pitch +
				(x * BLOCK_WIDTH) + MAX_VECTOR;
		}
	}
}

unsigned ZMBVEncoder::neededSize() const
{
	unsigned f = pixelSize;
	f = f * width * height + 2 * (1 + (width / 8)) * (1 + (height / 8)) + 1024;
	return f + f / 1000;
}

template<typename P>
unsigned ZMBVEncoder::possibleBlock(int vx, int vy, unsigned offset)
{
	int ret = 0;
	auto* pOld = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pNew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	for (unsigned y = 0; y < BLOCK_HEIGHT; y += 4) {
		for (unsigned x = 0; x < BLOCK_WIDTH; x += 4) {
			if (pOld[x] != pNew[x]) ++ret;
		}
		pOld += pitch * 4;
		pNew += pitch * 4;
	}
	return ret;
}

template<typename P>
unsigned ZMBVEncoder::compareBlock(int vx, int vy, unsigned offset)
{
	int ret = 0;
	auto* pOld = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pNew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	repeat(BLOCK_HEIGHT, [&] {
		for (auto x : xrange(BLOCK_WIDTH)) {
			if (pOld[x] != pNew[x]) ++ret;
		}
		pOld += pitch;
		pNew += pitch;
	});
	return ret;
}

template<typename P>
void ZMBVEncoder::addXorBlock(
	const PixelOperations<P>& pixelOps, int vx, int vy, unsigned offset, unsigned& workUsed)
{
	using LE_P = typename Endian::Little<P>::type;

	auto* pOld = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pNew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	repeat(BLOCK_HEIGHT, [&] {
		for (auto x : xrange(BLOCK_WIDTH)) {
			P pXor = pNew[x] ^ pOld[x];
			writePixel(pixelOps, pXor, *reinterpret_cast<LE_P*>(&work[workUsed]));
			workUsed += sizeof(P);
		}
		pOld += pitch;
		pNew += pitch;
	});
}

template<typename P>
void ZMBVEncoder::addXorFrame(const PixelFormat& pixelFormat, unsigned& workUsed)
{
	PixelOperations<P> pixelOps(pixelFormat);
	auto* vectors = reinterpret_cast<int8_t*>(&work[workUsed]);

	unsigned xBlocks = width / BLOCK_WIDTH;
	unsigned yBlocks = height / BLOCK_HEIGHT;
	unsigned blockcount = xBlocks * yBlocks;

	// Align the following xor data on 4 byte boundary
	workUsed = (workUsed + blockcount * 2 + 3) & ~3;

	int bestVx = 0;
	int bestVy = 0;
	for (auto b : xrange(blockcount)) {
		unsigned offset = blockOffsets[b];
		// first try best vector of previous block
		unsigned bestchange = compareBlock<P>(bestVx, bestVy, offset);
		if (bestchange >= 4) {
			int possibles = 64;
			for (const auto& v : vectorTable) {
				if (possibleBlock<P>(v.x, v.y, offset) < 4) {
					unsigned testchange = compareBlock<P>(v.x, v.y, offset);
					if (testchange < bestchange) {
						bestchange = testchange;
						bestVx = v.x;
						bestVy = v.y;
						if (bestchange < 4) break;
					}
					--possibles;
					if (possibles == 0) break;
				}
			}
		}
		vectors[b * 2 + 0] = (bestVx << 1);
		vectors[b * 2 + 1] = (bestVy << 1);
		if (bestchange) {
			vectors[b * 2 + 0] |= 1;
			addXorBlock<P>(pixelOps, bestVx, bestVy, offset, workUsed);
		}
	}
}

template<typename P>
void ZMBVEncoder::addFullFrame(const PixelFormat& pixelFormat, unsigned& workUsed)
{
	using LE_P = typename Endian::Little<P>::type;

	PixelOperations<P> pixelOps(pixelFormat);
	auto* readFrame =
		&newframe[pixelSize * (MAX_VECTOR + MAX_VECTOR * pitch)];
	repeat(height, [&] {
		auto* pixelsIn  = reinterpret_cast<P*>   (readFrame);
		auto* pixelsOut = reinterpret_cast<LE_P*>(&work[workUsed]);
		for (auto x : xrange(width)) {
			writePixel(pixelOps, pixelsIn[x], pixelsOut[x]);
		}
		readFrame += pitch * sizeof(P);
		workUsed += width * sizeof(P);
	});
}

const void* ZMBVEncoder::getScaledLine(FrameSource* frame, unsigned y, void* workBuf_) const
{
#if HAVE_32BPP
	if (pixelSize == 4) { // 32bpp
		auto* workBuf = static_cast<uint32_t*>(workBuf_);
		switch (height) {
		case 240:
			return frame->getLinePtr320_240(y, workBuf);
		case 480:
			return frame->getLinePtr640_480(y, workBuf);
		case 720:
			return frame->getLinePtr960_720(y, workBuf);
		default:
			UNREACHABLE;
		}
	}
#endif
#if HAVE_16BPP
	if (pixelSize == 2) { // 15bpp or 16bpp
		auto* workBuf = static_cast<uint16_t*>(workBuf_);
		switch (height) {
		case 240:
			return frame->getLinePtr320_240(y, workBuf);
		case 480:
			return frame->getLinePtr640_480(y, workBuf);
		case 720:
			return frame->getLinePtr960_720(y, workBuf);
		default:
			UNREACHABLE;
		}
	}
#endif
	UNREACHABLE;
	return nullptr; // avoid warning
}

std::span<const uint8_t> ZMBVEncoder::compressFrame(bool keyFrame, FrameSource* frame)
{
	std::swap(newframe, oldframe); // replace oldframe with newframe

	// Reset the work buffer
	unsigned workUsed = 0;
	unsigned writeDone = 1;
	uint8_t* writeBuf = output.data();

	output[0] = 0; // first byte contains info about this frame
	if (keyFrame) {
		output[0] |= FLAG_KEYFRAME;
		auto* header = reinterpret_cast<KeyframeHeader*>(
			writeBuf + writeDone);
		header->high_version = DBZV_VERSION_HIGH;
		header->low_version = DBZV_VERSION_LOW;
		header->compression = COMPRESSION_ZLIB;
		header->format = format;
		header->blockwidth = BLOCK_WIDTH;
		header->blockheight = BLOCK_HEIGHT;
		writeDone += sizeof(KeyframeHeader);
		deflateReset(&zstream); // restart deflate
	}

	// copy lines (to add black border)
	unsigned linePitch = pitch * pixelSize;
	unsigned lineWidth = width * pixelSize;
	uint8_t* dest =
		&newframe[pixelSize * (MAX_VECTOR + MAX_VECTOR * pitch)];
	for (auto i : xrange(height)) {
		const auto* scaled = getScaledLine(frame, i, dest);
		if (scaled != dest) memcpy(dest, scaled, lineWidth);
		dest += linePitch;
	}

	// Add the frame data.
	if (keyFrame) {
		// Key frame: full frame data.
		switch (pixelSize) {
#if HAVE_16BPP
		case 2:
			addFullFrame<uint16_t>(frame->getPixelFormat(), workUsed);
			break;
#endif
#if HAVE_32BPP
		case 4:
			addFullFrame<uint32_t>(frame->getPixelFormat(), workUsed);
			break;
#endif
		default:
			UNREACHABLE;
		}
	} else {
		// Non-key frame: delta frame data.
		switch (pixelSize) {
#if HAVE_16BPP
		case 2:
			addXorFrame<uint16_t>(frame->getPixelFormat(), workUsed);
			break;
#endif
#if HAVE_32BPP
		case 4:
			addXorFrame<uint32_t>(frame->getPixelFormat(), workUsed);
			break;
#endif
		default:
			UNREACHABLE;
		}
	}
	// Compress the frame data with zlib.
	zstream.next_in = work.data();
	zstream.avail_in = workUsed;
	zstream.total_in = 0;

	zstream.next_out = static_cast<Bytef*>(writeBuf + writeDone);
	zstream.avail_out = outputSize - writeDone;
	zstream.total_out = 0;
	auto r = deflate(&zstream, Z_SYNC_FLUSH);
	assert(r == Z_OK); (void)r;

	return {output.data(), writeDone + zstream.total_out};
}

} // namespace openmsx
