// $Id$

// Code based on DOSBox-0.65

#ifndef AVIWRITER_HH
#define AVIWRITER_HH

#include <string>
#include <vector>
#include <memory>
#include <cstdio>

namespace openmsx {

class ZMBVEncoder;

class AviWriter
{
public:
	AviWriter(const std::string& filename, unsigned width, unsigned height,
	          unsigned bpp, unsigned freq);
	~AviWriter();
	void addFrame(const void** lineData, unsigned samples, short* sampleData);
	void setFps(double fps);

private:
	void addAviChunk(const char* tag, unsigned size, void* data, unsigned flags);

	FILE* file;
	std::auto_ptr<ZMBVEncoder> codec;
	std::vector<unsigned char> index;

	double fps;
	const unsigned width;
	const unsigned height;
	const unsigned audiorate;

	unsigned frames;
	unsigned audiowritten;
	unsigned written;
};

} // namespace openmsx

#endif
