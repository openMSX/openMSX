// Code based on DOSBox-0.65

#ifndef AVIWRITER_HH
#define AVIWRITER_HH

#include "ZMBVEncoder.hh"
#include "File.hh"
#include "endian.hh"
#include <cstdint>
#include <vector>

namespace openmsx {

class Filename;
class FrameSource;

class AviWriter
{
public:
	AviWriter(const Filename& filename, unsigned width, unsigned height,
	          unsigned bpp, unsigned channels, unsigned freq);
	~AviWriter();
	void addFrame(FrameSource* frame, unsigned samples, int16_t* sampleData);
	void setFps(float fps_) { fps = fps_; }

private:
	void addAviChunk(const char* tag, size_t size, const void* data, unsigned flags);

private:
	File file;
	ZMBVEncoder codec;
	std::vector<Endian::L32> index;

	float fps;
	const unsigned width;
	const unsigned height;
	const unsigned channels;
	const unsigned audiorate;

	unsigned frames;
	unsigned audiowritten;
	unsigned written;
};

} // namespace openmsx

#endif
