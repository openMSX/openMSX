// $Id$

#include "WavWriter.hh"
#include "File.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>

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

WavWriter::WavWriter(const Filename& filename,
                     unsigned channels, unsigned bits, unsigned frequency)
	: file(new File(filename, "wb"))
	, bytes(0)
{
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

	*reinterpret_cast<short*>   (header + 22) = litEnd_16(channels);
	*reinterpret_cast<unsigned*>(header + 24) = litEnd_32(frequency);
	*reinterpret_cast<unsigned*>(header + 28) = litEnd_32((channels * frequency * bits) / 8);
	*reinterpret_cast<short*>   (header + 32) = litEnd_16((channels * bits) / 8);
	*reinterpret_cast<short*>   (header + 34) = litEnd_16(bits);

	file->write(header, sizeof(header));
}

WavWriter::~WavWriter()
{
	try {
		// data chunk must have an even number of bytes
		if (bytes & 1) {
			unsigned char pad = 0;
			file->write(&pad, 1);
		}

		flush(); // write header
	} catch (MSXException&) {
		// ignore, can't throw from destructor
	}
}

void WavWriter::write8mono(const unsigned char* val, unsigned len)
{
	file->write(val, len);
	bytes += len;
}

void WavWriter::write16stereo(const short* buffer, unsigned samples)
{
	unsigned size = 2 * sizeof(short) * samples;
	if (OPENMSX_BIGENDIAN) {
		VLA(short, buf, 2 * samples);
		for (unsigned i = 0; i < samples; ++i) {
			buf[2 * i + 0] = litEnd_16(buffer[2 * i + 0]);
			buf[2 * i + 1] = litEnd_16(buffer[2 * i + 1]);
		}
		file->write(buf, size);
	} else {
		file->write(buffer, size);
	}
	bytes += size;
}

void WavWriter::write16mono(const int* buffer, unsigned samples, int amp)
{
	VLA(short, buf, samples);
	for (unsigned i = 0; i < samples; ++i) {
		buf[i] = litEnd_16(Math::clipIntToShort(buffer[i] * amp));
	}
	unsigned size = sizeof(short) * samples;
	file->write(buf, size);
	bytes += size;
}

void WavWriter::write16stereo(const int* buffer, unsigned samples, int amp)
{
	write16mono(buffer, 2 * samples, amp);
}

bool WavWriter::isEmpty() const
{
	return bytes == 0;
}

void WavWriter::flush()
{
	// round totalsize up to next even number
	unsigned totalsize = litEnd_32((bytes + 44 - 8 + 1) & ~1);
	unsigned wavSize = litEnd_32(bytes);

	file->seek(4);
	file->write(&totalsize, 4);
	file->seek(40);
	file->write(&wavSize, 4);
	file->seek(file->getSize()); // SEEK_END
	file->flush();
}

} // namespace openmsx
