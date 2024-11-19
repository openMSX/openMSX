// Code based on DOSBox-0.65

#include "ZMBVEncoder.hh"

#include "FrameSource.hh"
#include "PixelOperations.hh"

#include "cstd.hh"
#include "endian.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "unreachable.hh"

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <tuple>

namespace openmsx {

static constexpr uint8_t DBZV_VERSION_HIGH = 0;
static constexpr uint8_t DBZV_VERSION_LOW = 1;
static constexpr uint8_t COMPRESSION_ZLIB = 1;
static constexpr unsigned MAX_VECTOR = 16;
static constexpr unsigned BLOCK_WIDTH  = MAX_VECTOR;
static constexpr unsigned BLOCK_HEIGHT = MAX_VECTOR;
static constexpr unsigned FLAG_KEYFRAME = 0x01;

struct CodecVector {
	int8_t x;
	int8_t y;
};

static constexpr unsigned VECTOR_TAB_SIZE =
	1 +                                       // center
	8 * MAX_VECTOR +                          // horizontal, vertical, diagonal
	MAX_VECTOR * MAX_VECTOR - 2 * MAX_VECTOR; // rest (only MAX_VECTOR/2)

static constexpr auto vectorTable = [] {
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
	uint8_t blockWidth;
	uint8_t blockHeight;
};


static inline void writePixel(
	unsigned pixel, Endian::L32& dest)
{
	PixelOperations pixelOps;
	unsigned r = pixelOps.red(pixel);
	unsigned g = pixelOps.green(pixel);
	unsigned b = pixelOps.blue(pixel);
	dest = (r << 16) | (g <<  8) |  b;
}


ZMBVEncoder::ZMBVEncoder(unsigned width_, unsigned height_)
	: width(width_)
	, height(height_)
{
	setupBuffers();
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

void ZMBVEncoder::setupBuffers()
{
	static constexpr size_t pixelSize = sizeof(Pixel);

	pitch = width + 2 * MAX_VECTOR;
	auto bufSize = (height + 2 * MAX_VECTOR) * pitch * pixelSize + 2048;

	oldFrame.resize(bufSize);
	newFrame.resize(bufSize);
	ranges::fill(std::span{oldFrame}, 0);
	ranges::fill(std::span{newFrame}, 0);
	work.resize(bufSize);
	outputSize = neededSize();
	output.resize(outputSize);

	assert((width  % BLOCK_WIDTH ) == 0);
	assert((height % BLOCK_HEIGHT) == 0);
	size_t xBlocks = width / BLOCK_WIDTH;
	size_t yBlocks = height / BLOCK_HEIGHT;
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
	static constexpr unsigned pixelSize = sizeof(Pixel);
	unsigned f = pixelSize * width * height + 2 * (1 + (width / 8)) * (1 + (height / 8)) + 1024;
	return f + f / 1000;
}

unsigned ZMBVEncoder::possibleBlock(int vx, int vy, size_t offset)
{
	int ret = 0;
	const auto* pOld = &(std::bit_cast<const Pixel*>(oldFrame.data()))[offset + (vy * pitch) + vx];
	const auto* pNew = &(std::bit_cast<const Pixel*>(newFrame.data()))[offset];
	for (unsigned y = 0; y < BLOCK_HEIGHT; y += 4) {
		for (unsigned x = 0; x < BLOCK_WIDTH; x += 4) {
			if (pOld[x] != pNew[x]) ++ret;
		}
		pOld += pitch * 4;
		pNew += pitch * 4;
	}
	return ret;
}

unsigned ZMBVEncoder::compareBlock(int vx, int vy, size_t offset)
{
	int ret = 0;
	const auto* pOld = &(std::bit_cast<const Pixel*>(oldFrame.data()))[offset + (vy * pitch) + vx];
	const auto* pNew = &(std::bit_cast<const Pixel*>(newFrame.data()))[offset];
	repeat(BLOCK_HEIGHT, [&] {
		for (auto x : xrange(BLOCK_WIDTH)) {
			if (pOld[x] != pNew[x]) ++ret;
		}
		pOld += pitch;
		pNew += pitch;
	});
	return ret;
}

void ZMBVEncoder::addXorBlock(int vx, int vy, size_t offset, unsigned& workUsed)
{
	using LE_P = typename Endian::Little<Pixel>::type;

	const auto* pOld = &(std::bit_cast<const Pixel*>(oldFrame.data()))[offset + (vy * pitch) + vx];
	const auto* pNew = &(std::bit_cast<const Pixel*>(newFrame.data()))[offset];
	repeat(BLOCK_HEIGHT, [&] {
		for (auto x : xrange(BLOCK_WIDTH)) {
			auto pXor = pNew[x] ^ pOld[x];
			writePixel(pXor, *std::bit_cast<LE_P*>(&work[workUsed]));
			workUsed += sizeof(Pixel);
		}
		pOld += pitch;
		pNew += pitch;
	});
}

void ZMBVEncoder::addXorFrame(unsigned& workUsed)
{
	auto* vectors = std::bit_cast<int8_t*>(&work[workUsed]);

	unsigned xBlocks = width / BLOCK_WIDTH;
	unsigned yBlocks = height / BLOCK_HEIGHT;
	unsigned blockCount = xBlocks * yBlocks;

	// Align the following xor data on 4 byte boundary
	workUsed = (workUsed + blockCount * 2 + 3) & ~3;

	int bestVx = 0;
	int bestVy = 0;
	for (auto b : xrange(blockCount)) {
		auto offset = blockOffsets[b];
		// first try best vector of previous block
		unsigned bestChange = compareBlock(bestVx, bestVy, offset);
		if (bestChange >= 4) {
			int possibles = 64;
			for (const auto& v : vectorTable) {
				if (possibleBlock(v.x, v.y, offset) < 4) {
					if (auto testChange = compareBlock(v.x, v.y, offset);
					    testChange < bestChange) {
						bestChange = testChange;
						bestVx = narrow<int>(v.x);
						bestVy = narrow<int>(v.y);
						if (bestChange < 4) break;
					}
					--possibles;
					if (possibles == 0) break;
				}
			}
		}
		vectors[b * 2 + 0] = narrow<int8_t>(bestVx << 1);
		vectors[b * 2 + 1] = narrow<int8_t>(bestVy << 1);
		if (bestChange) {
			vectors[b * 2 + 0] |= 1;
			addXorBlock(bestVx, bestVy, offset, workUsed);
		}
	}
}

void ZMBVEncoder::addFullFrame(unsigned& workUsed)
{
	using LE_P = typename Endian::Little<Pixel>::type;
	static constexpr size_t pixelSize = sizeof(Pixel);

	auto* readFrame =
		&newFrame[pixelSize * (MAX_VECTOR + MAX_VECTOR * pitch)];
	repeat(height, [&] {
		const auto* pixelsIn = std::bit_cast<const Pixel*>(readFrame);
		auto* pixelsOut = std::bit_cast<LE_P*>(&work[workUsed]);
		for (auto x : xrange(width)) {
			writePixel(pixelsIn[x], pixelsOut[x]);
		}
		readFrame += pitch * sizeof(Pixel);
		workUsed += narrow<unsigned>(width * sizeof(Pixel));
	});
}

const ZMBVEncoder::Pixel* ZMBVEncoder::getScaledLine(const FrameSource* frame, unsigned y, Pixel* workBuf) const
{
	switch (height) {
	case 240:
		return frame->getLinePtr320_240(y, std::span<uint32_t, 320>(workBuf, 320)).data();
	case 480:
		return frame->getLinePtr640_480(y, std::span<uint32_t, 640>(workBuf, 640)).data();
	case 720:
		return frame->getLinePtr960_720(y, std::span<uint32_t, 960>(workBuf, 960)).data();
	default:
		UNREACHABLE;
	}
}

std::span<const uint8_t> ZMBVEncoder::compressFrame(bool keyFrame, const FrameSource* frame)
{
	std::swap(newFrame, oldFrame); // replace oldFrame with newFrame

	// Reset the work buffer
	unsigned workUsed = 0;
	unsigned writeDone = 1;
	uint8_t* writeBuf = output.data();

	output[0] = 0; // first byte contains info about this frame
	if (keyFrame) {
		static const uint8_t ZMBV_FORMAT_32BPP = 8;

		output[0] |= FLAG_KEYFRAME;
		auto* header = std::bit_cast<KeyframeHeader*>(
			writeBuf + writeDone);
		header->high_version = DBZV_VERSION_HIGH;
		header->low_version = DBZV_VERSION_LOW;
		header->compression = COMPRESSION_ZLIB;
		header->format = ZMBV_FORMAT_32BPP;
		header->blockWidth = BLOCK_WIDTH;
		header->blockHeight = BLOCK_HEIGHT;
		writeDone += sizeof(KeyframeHeader);
		deflateReset(&zstream); // restart deflate
	}

	// copy lines (to add black border)
	static constexpr size_t pixelSize = sizeof(Pixel);
	auto linePitch = pitch * pixelSize;
	auto lineWidth = size_t(width) * pixelSize;
	uint8_t* dest =
		&newFrame[pixelSize * (MAX_VECTOR + MAX_VECTOR * pitch)];
	for (auto i : xrange(height)) {
		const auto* scaled = std::bit_cast<const uint8_t*>(
			getScaledLine(frame, i, std::bit_cast<Pixel*>(dest)));
		if (scaled != dest) memcpy(dest, scaled, lineWidth);
		dest += linePitch;
	}

	// Add the frame data.
	if (keyFrame) {
		// Key frame: full frame data.
		addFullFrame(workUsed);
	} else {
		// Non-key frame: delta frame data.
		addXorFrame(workUsed);
	}
	// Compress the frame data with zlib.
	zstream.next_in = work.data();
	zstream.avail_in = workUsed;
	zstream.total_in = 0;

	zstream.next_out = std::bit_cast<Bytef*>(writeBuf + writeDone);
	zstream.avail_out = outputSize - writeDone;
	zstream.total_out = 0;
	auto r = deflate(&zstream, Z_SYNC_FLUSH);
	assert(r == Z_OK); (void)r;

	return {output.data(), writeDone + zstream.total_out};
}

} // namespace openmsx
