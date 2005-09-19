// $Id$

#include "WavWriter.hh"
#include "MSXException.hh"
#include "build-info.hh"

namespace openmsx {

static inline unsigned short litEnd_16(unsigned short val)
{
	return (OPENMSX_BIGENDIAN)
	       ? (((val & 0xFF00) >> 8) |
	          ((val & 0x00FF) << 8))
	       : val;
}
static inline unsigned litEnd_32(unsigned val)
{
	return (OPENMSX_BIGENDIAN)
	       ? (((val & 0xFF000000) >> 24) |
	          ((val & 0x00FF0000) >>  8) |
	          ((val & 0x0000FF00) <<  8) |
	          ((val & 0x000000FF) << 24))
	       : val;
}

WavWriter::WavWriter(const std::string& filename,
                     unsigned channels, unsigned bits, unsigned frequency)
	: bytes(0)
{
	wavfp = fopen(filename.c_str(), "wb");
	if (!wavfp) {
		throw MSXException(
			"Couldn't open file for writing: " + filename);
	}

	// write wav header
	char header[44] = {
		'R', 'I', 'F', 'F', //
		0, 0, 0, 0,         // total size (filled in later)
		'W', 'A', 'V', 'E', //
		'f', 'm', 't', ' ', //
		16, 0, 0, 0,        // size of fmt block
		1, 0,               // format tag = 1
		2, 0,               // nb of channels (filled in)
		0, 0, 0, 0,         // samples per second (filled in)
		0, 0, 0, 0,         // avg bytes per second (filled in)
		0, 0,               // block align (filled in)
		0, 0,               // bits per sample (filled in)
		'd', 'a', 't', 'a', //
		0, 0, 0, 0,         // size of data block (filled in later)
	};

	*(short*)   (header + 22) = litEnd_16(channels);
	*(unsigned*)(header + 24) = litEnd_32(frequency);
	*(unsigned*)(header + 28) = litEnd_32((channels * frequency * bits) / 8);
	*(short*)   (header + 32) = litEnd_16((channels * bits) / 8);
	*(short*)   (header + 34) = litEnd_16(bits);

	fwrite(header, sizeof(header), 1, wavfp);
}

WavWriter::~WavWriter()
{
	unsigned totalsize = litEnd_32(bytes + 44);
	unsigned wavSize   = litEnd_32(bytes);
	
	fseek(wavfp,  4, SEEK_SET);
	fwrite(&totalsize, 4, 1, wavfp);
	fseek(wavfp, 40, SEEK_SET);
	fwrite(&wavSize,   4, 1, wavfp);

	fclose(wavfp);
}

void WavWriter::write8mono(unsigned char val)
{
	fwrite(&val, 1, 1, wavfp);
	bytes += 1;
}

void WavWriter::write16stereo(short left, short right)
{
	short buf[2];
	buf[0] = litEnd_16(left);
	buf[1] = litEnd_16(right);
	fwrite(buf, 4, 1, wavfp);
	bytes += 4;
}

} // namespace openmsx
