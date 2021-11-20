// Code based on DOSBox-0.65

#ifndef ZMBVENCODER_HH
#define ZMBVENCODER_HH

#include "PixelFormat.hh"
#include "MemBuffer.hh"
#include "aligned.hh"
#include "span.hh"
#include <cstdint>
#include <zlib.h>

namespace openmsx {

class FrameSource;
template<typename P> class PixelOperations;

class ZMBVEncoder
{
public:
	static constexpr const char CODEC_4CC[5] = "ZMBV"; // 4 + zero-terminator

	ZMBVEncoder(unsigned width, unsigned height, unsigned bpp);

	[[nodiscard]] span<const uint8_t> compressFrame(bool keyFrame, FrameSource* frame);

private:
	enum Format {
		ZMBV_FORMAT_16BPP = 6,
		ZMBV_FORMAT_32BPP = 8
	};

	void setupBuffers(unsigned bpp);
	[[nodiscard]] unsigned neededSize() const;
	template<typename P> void addFullFrame(const PixelFormat& pixelFormat, unsigned& workUsed);
	template<typename P> void addXorFrame (const PixelFormat& pixelFormat, unsigned& workUsed);
	template<typename P> [[nodiscard]] unsigned possibleBlock(int vx, int vy, unsigned offset);
	template<typename P> [[nodiscard]] unsigned compareBlock(int vx, int vy, unsigned offset);
	template<typename P> void addXorBlock(
		const PixelOperations<P>& pixelOps, int vx, int vy,
		unsigned offset, unsigned& workUsed);
	[[nodiscard]] const void* getScaledLine(FrameSource* frame, unsigned y, void* workBuf) const;

private:
	MemBuffer<uint8_t, SSE_ALIGNMENT> oldframe;
	MemBuffer<uint8_t, SSE_ALIGNMENT> newframe;
	MemBuffer<uint8_t, SSE_ALIGNMENT> work;
	MemBuffer<uint8_t> output;
	MemBuffer<unsigned> blockOffsets;
	unsigned outputSize;

	z_stream zstream;

	const unsigned width;
	const unsigned height;
	unsigned pitch;
	unsigned pixelSize;
	Format format;
};

} // namespace openmsx

#endif
