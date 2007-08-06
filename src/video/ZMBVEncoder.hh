// $Id: $

// Code based on DOSBox-0.65

#ifndef ZMBVENCODER_HH
#define ZMBVENCODER_HH

#include <vector>
#include <zlib.h>

namespace openmsx {

class ZMBVEncoder
{
public:
	static const char* CODEC_4CC;

	ZMBVEncoder(unsigned width, unsigned height, unsigned bpp);
	~ZMBVEncoder();

	void compressFrame(bool keyFrame, const void** lineData,
	                   void*& buffer, unsigned& written);

private:
	enum Format {
		ZMBV_FORMAT_15BPP = 5,
		ZMBV_FORMAT_16BPP = 6,
		ZMBV_FORMAT_32BPP = 8
	};

	void createVectorTable();
	void setupBuffers(unsigned bpp);
	unsigned neededSize();
	template<class P> void addXorFrame();
	template<class P> unsigned possibleBlock(int vx, int vy, unsigned offset);
	template<class P> unsigned compareBlock(int vx, int vy, unsigned offset);
	template<class P> void addXorBlock(int vx, int vy, unsigned offset);

	struct CodecVector {
		CodecVector(int x_, int y_) : x(x_), y(y_) {}
		int x;
		int y;
	};
	struct KeyframeHeader {
		unsigned char high_version;
		unsigned char low_version;
		unsigned char compression;
		unsigned char format;
		unsigned char blockwidth;
		unsigned char blockheight;
	};

	unsigned char* oldframe;
	unsigned char* newframe;
	unsigned char* work;
	unsigned char* output;
	unsigned outputSize;
	unsigned workUsed;

	std::vector<unsigned> blockOffsets;
	std::vector<CodecVector> vectorTable;
	z_stream zstream;

	unsigned width;
	unsigned height;
	unsigned pitch;
	unsigned pixelsize;
	Format format;
};

} // namespace openmsx

#endif
