// Code based on DOSBox-0.65

#ifndef ZMBVENCODER_HH
#define ZMBVENCODER_HH

#include "MemBuffer.hh"
#include "aligned.hh"
#include <concepts>
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

	[[nodiscard]] std::span<const uint8_t> compressFrame(bool keyFrame, FrameSource* frame);

private:
	void setupBuffers();
	[[nodiscard]] unsigned neededSize() const;
	void addFullFrame(unsigned& workUsed);
	void addXorFrame (unsigned& workUsed);
	[[nodiscard]] unsigned possibleBlock(int vx, int vy, size_t offset);
	[[nodiscard]] unsigned compareBlock(int vx, int vy, size_t offset);
	void addXorBlock(int vx, int vy, size_t offset, unsigned& workUsed);
	[[nodiscard]] const void* getScaledLine(const FrameSource* frame, unsigned y, void* workBuf) const;

private:
	MemBuffer<uint8_t, SSE_ALIGNMENT> oldFrame;
	MemBuffer<uint8_t, SSE_ALIGNMENT> newFrame;
	MemBuffer<uint8_t, SSE_ALIGNMENT> work;
	MemBuffer<uint8_t> output;
	MemBuffer<size_t> blockOffsets;
	unsigned outputSize;

	z_stream zstream;

	const unsigned width;
	const unsigned height;
	size_t pitch;
};

} // namespace openmsx

#endif
