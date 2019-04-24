#include "WavData.hh"
#include "File.hh"
#include "MSXException.hh"
#include "endian.hh"

namespace openmsx {

WavData::WavData(const std::string& filename)
{
	File file(filename);

	struct WavHeader {
		char riffID[4];
		Endian::L32 riffSize;
		char riffType[4];
		char fmtID[4];
		Endian::L32 fmtSize;
		Endian::L16 wFormatTag;
		Endian::L16 wChannels;
		Endian::L32 dwSamplesPerSec;
		Endian::L32 dwAvgBytesPerSec;
		Endian::L16 wBlockAlign;
		Endian::L16 wBitsPerSample;
	} header;

	struct DataHeader {
		char dataID[4];
		Endian::L32 size;
	} data;

	// Read and check header
	file.read(&header, sizeof(WavHeader));
	if (memcmp(header.riffID, "RIFF", 4) ||
	    memcmp(header.riffType, "WAVE", 4) ||
	    memcmp(header.fmtID, "fmt ", 4)) {
		throw MSXException("Invalid WAV");
	}
	if ((header.wFormatTag != 1) || ((header.wBitsPerSample != 8) && (header.wBitsPerSample != 16))) {
		throw MSXException("WAV format unsupported, must be 8 or 16 bit PCM");
	}
	// Skip any extra format bytes
	file.seek(file.getPos() + (header.fmtSize - (sizeof(WavHeader) - 20)));

	// Find 'data' chunk
	for(;;) {
		// Read chunk header
		file.read(&data, sizeof(DataHeader));
		if (!memcmp(data.dataID, "data", 4)) break;

		// Skip chunk
		file.seek(file.getPos() + data.size);
	}
	freq = header.dwSamplesPerSec;
	length = data.size / ((header.wBitsPerSample / 8) * header.wChannels);
	buffer.resize(length);

	// Read sample data
	auto pos = file.getPos();
	for (unsigned i = 0; i < length; ++i) {
		Endian::L16 sample;

		// TODO implement multi-channel to mono conversion
		file.read(&sample, header.wBitsPerSample / 8);
		buffer[i] = (header.wBitsPerSample == 8)
		          ? (int16_t(sample - 0x80) & 0xFF) << 8
		          : int16_t(sample);

		pos += (header.wBitsPerSample / 8) * header.wChannels;
		file.seek(pos);
	}
}

int16_t WavData::getSample(unsigned pos) const
{
	if (pos < length) {
		return buffer[pos];
	}
	return 0;
}

} // namespace openmsx
