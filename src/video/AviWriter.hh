// $Id: $

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
	          unsigned bpp, double fps, unsigned freq);
	~AviWriter();
	void addFrame(const void** lineData, unsigned samples, short* sampleData);

private:
	void addAviChunk(const char* tag, unsigned size, void* data, unsigned flags);

	const unsigned width;
	const unsigned height;
	const unsigned audiorate;
	const double fps;

	unsigned frames;
	unsigned audiowritten;
	unsigned written;
	FILE* file;
	std::auto_ptr<ZMBVEncoder> codec;
	std::vector<unsigned char> index;
};

} // namespace openmsx

#endif
