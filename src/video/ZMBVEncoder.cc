// Code based on DOSBox-0.65

#include "ZMBVEncoder.hh"
#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "endian.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace openmsx {

static const uint8_t DBZV_VERSION_HIGH = 0;
static const uint8_t DBZV_VERSION_LOW = 1;
static const uint8_t COMPRESSION_ZLIB = 1;
static const unsigned MAX_VECTOR = 16;
static const unsigned BLOCK_WIDTH  = MAX_VECTOR;
static const unsigned BLOCK_HEIGHT = MAX_VECTOR;
static const unsigned FLAG_KEYFRAME = 0x01;

struct CodecVector {
	float cost() const {
		float c = sqrtf(float(x * x + y * y));
		if ((x == 0) || (y == 0)) {
			// no penalty for purely horizontal/vertical offset
			c *= 1.0f;
		} else if (abs(x) == abs(y)) {
			// small penalty for pure diagonal
			c *= 2.0f;
		} else {
			// bigger penalty for 'random' direction
			c *= 4.0f;
		}
		return c;
	}
	int x;
	int y;
};
static inline bool operator<(const CodecVector& l, const CodecVector& r)
{
	return l.cost() < r.cost();
}

static const unsigned VECTOR_TAB_SIZE =
	1 +                                       // center
	8 * MAX_VECTOR +                          // horizontal, vertial, diagonal
	MAX_VECTOR * MAX_VECTOR - 2 * MAX_VECTOR; // rest (only MAX_VECTOR/2)
CodecVector vectorTable[VECTOR_TAB_SIZE];

struct KeyframeHeader {
	uint8_t high_version;
	uint8_t low_version;
	uint8_t compression;
	uint8_t format;
	uint8_t blockwidth;
	uint8_t blockheight;
};

const char* ZMBVEncoder::CODEC_4CC = "ZMBV";


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

static void createVectorTable()
{
	unsigned p = 0;
	// center
	vectorTable[p] = {0, 0};
	p += 1;
	// horizontal, vertial, diagonal
	for (int i = 1; i <= int(MAX_VECTOR); ++i) {
		vectorTable[p + 0] = { i, 0};
		vectorTable[p + 1] = {-i, 0};
		vectorTable[p + 2] = { 0, i};
		vectorTable[p + 3] = { 0,-i};
		vectorTable[p + 4] = { i, i};
		vectorTable[p + 5] = {-i, i};
		vectorTable[p + 6] = { i,-i};
		vectorTable[p + 7] = {-i,-i};
		p += 8;
	}
	// rest
	for (int y = 1; y <= int(MAX_VECTOR / 2); ++y) {
		for (int x = 1; x <= int(MAX_VECTOR / 2); ++x) {
			if (x == y) continue; // already have diagonal
			vectorTable[p + 0] = { x, y};
			vectorTable[p + 1] = {-x, y};
			vectorTable[p + 2] = { x,-y};
			vectorTable[p + 3] = {-x,-y};
			p += 4;
		}
	}
	assert(p == VECTOR_TAB_SIZE);

	ranges::sort(vectorTable);
}

ZMBVEncoder::ZMBVEncoder(unsigned width_, unsigned height_, unsigned bpp)
	: width(width_)
	, height(height_)
{
	setupBuffers(bpp);
	createVectorTable();
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
	unsigned xblocks = width / BLOCK_WIDTH;
	unsigned yblocks = height / BLOCK_HEIGHT;
	blockOffsets.resize(xblocks * yblocks);
	for (unsigned y = 0; y < yblocks; ++y) {
		for (unsigned x = 0; x < xblocks; ++x) {
			blockOffsets[y * xblocks + x] =
				((y * BLOCK_HEIGHT) + MAX_VECTOR) * pitch +
				(x * BLOCK_WIDTH) + MAX_VECTOR;
		}
	}
}

unsigned ZMBVEncoder::neededSize()
{
	unsigned f = pixelSize;
	f = f * width * height + 2 * (1 + (width / 8)) * (1 + (height / 8)) + 1024;
	return f + f / 1000;
}

template<class P>
unsigned ZMBVEncoder::possibleBlock(int vx, int vy, unsigned offset)
{
	int ret = 0;
	auto* pold = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pnew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	for (unsigned y = 0; y < BLOCK_HEIGHT; y += 4) {
		for (unsigned x = 0; x < BLOCK_WIDTH; x += 4) {
			if (pold[x] != pnew[x]) ++ret;
		}
		pold += pitch * 4;
		pnew += pitch * 4;
	}
	return ret;
}

template<class P>
unsigned ZMBVEncoder::compareBlock(int vx, int vy, unsigned offset)
{
	int ret = 0;
	auto* pold = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pnew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	for (unsigned y = 0; y < BLOCK_HEIGHT; ++y) {
		for (unsigned x = 0; x < BLOCK_WIDTH; ++x) {
			if (pold[x] != pnew[x]) ++ret;
		}
		pold += pitch;
		pnew += pitch;
	}
	return ret;
}

template<class P>
void ZMBVEncoder::addXorBlock(
	const PixelOperations<P>& pixelOps, int vx, int vy, unsigned offset, unsigned& workUsed)
{
	using LE_P = typename Endian::Little<P>::type;

	auto* pold = &(reinterpret_cast<P*>(oldframe.data()))[offset + (vy * pitch) + vx];
	auto* pnew = &(reinterpret_cast<P*>(newframe.data()))[offset];
	for (unsigned y = 0; y < BLOCK_HEIGHT; ++y) {
		for (unsigned x = 0; x < BLOCK_WIDTH; ++x) {
			P pxor = pnew[x] ^ pold[x];
			writePixel(pixelOps, pxor, *reinterpret_cast<LE_P*>(&work[workUsed]));
			workUsed += sizeof(P);
		}
		pold += pitch;
		pnew += pitch;
	}
}

template<class P>
void ZMBVEncoder::addXorFrame(const SDL_PixelFormat& pixelFormat, unsigned& workUsed)
{
	PixelOperations<P> pixelOps(pixelFormat);
	auto* vectors = reinterpret_cast<int8_t*>(&work[workUsed]);

	unsigned xblocks = width / BLOCK_WIDTH;
	unsigned yblocks = height / BLOCK_HEIGHT;
	unsigned blockcount = xblocks * yblocks;

	// Align the following xor data on 4 byte boundary
	workUsed = (workUsed + blockcount * 2 + 3) & ~3;

	int bestvx = 0;
	int bestvy = 0;
	for (unsigned b = 0; b < blockcount; ++b) {
		unsigned offset = blockOffsets[b];
		// first try best vector of previous block
		unsigned bestchange = compareBlock<P>(bestvx, bestvy, offset);
		if (bestchange >= 4) {
			int possibles = 64;
			for (auto& v : vectorTable) {
				if (possibleBlock<P>(v.x, v.y, offset) < 4) {
					unsigned testchange = compareBlock<P>(v.x, v.y, offset);
					if (testchange < bestchange) {
						bestchange = testchange;
						bestvx = v.x;
						bestvy = v.y;
						if (bestchange < 4) break;
					}
					--possibles;
					if (possibles == 0) break;
				}
			}
		}
		vectors[b * 2 + 0] = (bestvx << 1);
		vectors[b * 2 + 1] = (bestvy << 1);
		if (bestchange) {
			vectors[b * 2 + 0] |= 1;
			addXorBlock<P>(pixelOps, bestvx, bestvy, offset, workUsed);
		}
	}
}

template<class P>
void ZMBVEncoder::addFullFrame(const SDL_PixelFormat& pixelFormat, unsigned& workUsed)
{
	using LE_P = typename Endian::Little<P>::type;

	PixelOperations<P> pixelOps(pixelFormat);
	auto* readFrame =
		&newframe[pixelSize * (MAX_VECTOR + MAX_VECTOR * pitch)];
	for (unsigned y = 0; y < height; ++y) {
		auto* pixelsIn  = reinterpret_cast<P*>   (readFrame);
		auto* pixelsOut = reinterpret_cast<LE_P*>(&work[workUsed]);
		for (unsigned x = 0; x < width; ++x) {
			writePixel(pixelOps, pixelsIn[x], pixelsOut[x]);
		}
		readFrame += pitch * sizeof(P);
		workUsed += width * sizeof(P);
	}
}

const void* ZMBVEncoder::getScaledLine(FrameSource* frame, unsigned y, void* workBuf_)
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

void ZMBVEncoder::compressFrame(bool keyFrame, FrameSource* frame,
                                void*& buffer, unsigned& written)
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
	for (unsigned i = 0; i < height; ++i) {
		auto* scaled = getScaledLine(frame, i, dest);
		if (scaled != dest) memcpy(dest, scaled, lineWidth);
		dest += linePitch;
	}

	// Add the frame data.
	if (keyFrame) {
		// Key frame: full frame data.
		switch (pixelSize) {
#if HAVE_16BPP
		case 2:
			addFullFrame<uint16_t>(frame->getSDLPixelFormat(), workUsed);
			break;
#endif
#if HAVE_32BPP
		case 4:
			addFullFrame<uint32_t>(frame->getSDLPixelFormat(), workUsed);
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
			addXorFrame<uint16_t>(frame->getSDLPixelFormat(), workUsed);
			break;
#endif
#if HAVE_32BPP
		case 4:
			addXorFrame<uint32_t>(frame->getSDLPixelFormat(), workUsed);
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

	buffer = output.data();
	written = writeDone + zstream.total_out;
}

} // namespace openmsx
