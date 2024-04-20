// Code based on DOSBox-0.65

#ifndef ZMBVENCODER_HH
#define ZMBVENCODER_HH

#include "MemBuffer.hh"
#include "aligned.hh"

#include <cstdint>
#include <span>
#include <string_view>

#include <zlib.h>

namespace openmsx {

class FrameSource;

class ZMBVEncoder
{
public:
	static constexpr std::string_view CODEC_4CC = "ZMBV";
	using Pixel = uint32_t;

	ZMBVEncoder(unsigned width, unsigned height);
	ZMBVEncoder(const ZMBVEncoder&) = delete;
	ZMBVEncoder(ZMBVEncoder&&) = delete;
	ZMBVEncoder& operator=(const ZMBVEncoder&) = delete;
	ZMBVEncoder& operator=(ZMBVEncoder&&) = delete;
	~ZMBVEncoder() = default;

	[[nodiscard]] std::span<const uint8_t> compressFrame(bool keyFrame, const FrameSource* frame);

private:
	void setupBuffers();
	[[nodiscard]] unsigned neededSize() const;
	void addFullFrame(unsigned& workUsed);
	void addXorFrame (unsigned& workUsed);
	[[nodiscard]] unsigned possibleBlock(int vx, int vy, size_t offset);
	[[nodiscard]] unsigned compareBlock(int vx, int vy, size_t offset);
	void addXorBlock(int vx, int vy, size_t offset, unsigned& workUsed);
	[[nodiscard]] const Pixel* getScaledLine(const FrameSource* frame, unsigned y, Pixel* workBuf) const;

private:
	MemBuffer<uint8_t, SSE_ALIGNMENT> oldFrame;
	MemBuffer<uint8_t, SSE_ALIGNMENT> newFrame;
	MemBuffer<uint8_t, SSE_ALIGNMENT> work;
	MemBuffer<uint8_t> output;
	MemBuffer<size_t> blockOffsets;
	unsigned outputSize;

	z_stream zstream;

	unsigned width;
	unsigned height;
	size_t pitch;
};

} // namespace openmsx

#endif
