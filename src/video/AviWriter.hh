// Code based on DOSBox-0.65

#ifndef AVIWRITER_HH
#define AVIWRITER_HH

#include "ZMBVEncoder.hh"

#include "File.hh"

#include "endian.hh"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class FrameSource;

class AviWriter
{
public:
	AviWriter(const std::string& filename, unsigned width, unsigned height,
	          unsigned channels, unsigned freq);
	~AviWriter();
	void addFrame(const FrameSource* video, std::span<const int16_t> audio);
	void setFps(float fps_) { fps = fps_; }

private:
	void addAviChunk(std::span<const char, 4> tag, std::span<const uint8_t> data, unsigned flags);

private:
	File file;
	std::string filename;
	ZMBVEncoder codec;
	std::vector<Endian::L32> index;

	float fps = 0.0f; // will be filled in later
	const uint32_t width;
	const uint32_t height;
	const uint32_t channels;
	const uint32_t audioRate;

	uint32_t frames = 0;
	uint32_t audioWritten = 0;
	uint32_t written = 0;
};

} // namespace openmsx

#endif
