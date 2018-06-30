#include "CasImage.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "Clock.hh"
#include "MSXException.hh"
#include "xrange.hh"
#include <cstring> // for memcmp

namespace openmsx {

// output settings

// a higher baudrate doesn't work anymore, but it is unclear why, because 4600
// baud should work (known from Speedsave 4000 and Turbo 5000 programs).
// 3765 still works on a Toshiba HX-10 and Philips NMS 8250, but not on a
// Panasonic FS-A1WSX, on which 3763 is the max. National CF-2000 has 3762 as
// the max. Let's take 3760 then as a safe value.
// UPDATE: that seems to break RUN"CAS:" type of programs. 3744 seems to work
// for those as well (we don't understand why yet)
static const unsigned BAUDRATE = 3744;
static const unsigned OUTPUT_FREQUENCY = 4 * BAUDRATE; // 4 samples per bit
// We oversample the audio signal for better sound quality (especially in
// combination with the hq resampler). Without oversampling the audio output
// could contain portions like this:
//   -1, +1, -1, +1, -1, +1, ...
// So it contains a signal at the Nyquist frequency. The hq resampler contains
// a low-pass filter, and (all practically implementable) filters cut off a
// portion of the spectrum near the Nyquist freq. So this high freq signal was
// lost after the hq resampler. After oversampling, the signal looks like this:
//   -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1, ...
// So every sample repeated 4 times.
static const unsigned AUDIO_OVERSAMPLE = 4;

// number of output bytes for silent parts
static const unsigned SHORT_SILENCE = OUTPUT_FREQUENCY * 1; // 1 second
static const unsigned LONG_SILENCE  = OUTPUT_FREQUENCY * 2; // 2 seconds

// number of 1-bits for headers
static const unsigned LONG_HEADER  = 16000 / 2;
static const unsigned SHORT_HEADER =  4000 / 2;

// headers definitions
static const byte CAS_HEADER   [ 8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
static const byte ASCII_HEADER [10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
static const byte BINARY_HEADER[10] = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
static const byte BASIC_HEADER [10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };


CasImage::CasImage(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	setFirstFileType(CassetteImage::UNKNOWN);
	convert(filename, filePool, cliComm);
}

int16_t CasImage::getSampleAt(EmuTime::param time)
{
	static const Clock<OUTPUT_FREQUENCY> zero(EmuTime::zero);
	unsigned pos = zero.getTicksTill(time);
	return pos < output.size() ? output[pos] * 256 : 0;
}

EmuTime CasImage::getEndTime() const
{
	Clock<OUTPUT_FREQUENCY> clk(EmuTime::zero);
	clk += unsigned(output.size());
	return clk.getTime();
}

unsigned CasImage::getFrequency() const
{
	return OUTPUT_FREQUENCY * AUDIO_OVERSAMPLE;
}

void CasImage::fillBuffer(unsigned pos, int** bufs, unsigned num) const
{
	size_t nbSamples = output.size();
	if ((pos / AUDIO_OVERSAMPLE) < nbSamples) {
		for (auto i : xrange(num)) {
			bufs[0][i] = ((pos / AUDIO_OVERSAMPLE) < nbSamples)
			           ? output[pos / AUDIO_OVERSAMPLE] * 256
			           : 0;
			++pos;
		}
	} else {
		bufs[0] = nullptr;
	}
}

void CasImage::write0()
{
	output.insert(end(output), { 127, 127, -127, -127 } );
}
void CasImage::write1()
{
	output.insert(end(output), { 127, -127, 127, -127 } );
}

// write a header signal
void CasImage::writeHeader(int s)
{
	for (int i = 0; i < s; ++i) {
		write1();
	}
}

// write silence
void CasImage::writeSilence(int s)
{
	output.insert(end(output), s, 0);
}

// write a byte
void CasImage::writeByte(byte b)
{
	// one start bit
	write0();
	// eight data bits
	for (auto i : xrange(8)) {
		if (b & (1 << i)) {
			write1();
		} else {
			write0();
		}
	}
	// two stop bits
	write1();
	write1();
}

// write data until a header is detected
bool CasImage::writeData(const byte* buf, size_t size, size_t& pos)
{
	bool eof = false;
	while ((pos + 8) <= size) {
		if (memcmp(&buf[pos], CAS_HEADER, 8) == 0) {
			return eof;
		}
		writeByte(buf[pos]);
		if (buf[pos] == 0x1A) {
			eof = true;
		}
		pos++;
	}
	while (pos < size) {
		writeByte(buf[pos++]);
	}
	return false;
}

void CasImage::convert(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	File file(filename);
	size_t size;
	const byte* buf = file.mmap(size);

	// search for a header in the .cas file
	bool issueWarning = false;
	bool headerFound = false;
	bool firstFile = true;
	size_t pos = 0;
	while ((pos + 8) <= size) {
		if (memcmp(&buf[pos], CAS_HEADER, 8) == 0) {
			// it probably works fine if a long header is used for every
			// header but since the msx bios makes a distinction between
			// them, we do also (hence a lot of code).
			headerFound = true;
			pos += 8;
			writeSilence(LONG_SILENCE);
			writeHeader(LONG_HEADER);
			if ((pos + 10) <= size) {
				// determine file type
				FileType type = CassetteImage::UNKNOWN;
				if (memcmp(&buf[pos], ASCII_HEADER, 10) == 0) {
					type = CassetteImage::ASCII;
				} else if (memcmp(&buf[pos], BINARY_HEADER, 10) == 0) {
					type = CassetteImage::BINARY;
				} else if (memcmp(&buf[pos], BASIC_HEADER, 10) == 0) {
					type = CassetteImage::BASIC;
				}
				if (firstFile) setFirstFileType(type);
				switch (type) {
					case CassetteImage::ASCII:
						writeData(buf, size, pos);
						bool eof;
						do {
							pos += 8;
							writeSilence(SHORT_SILENCE);
							writeHeader(SHORT_HEADER);
							eof = writeData(buf, size, pos);
						} while (!eof && ((pos + 8) <= size));
						break;
					case CassetteImage::BINARY:
					case CassetteImage::BASIC:
						writeData(buf, size, pos);
						writeSilence(SHORT_SILENCE);
						writeHeader(SHORT_HEADER);
						pos += 8;
						writeData(buf, size, pos);
						break;
					default:
						// unknown file type: using long header
						writeData(buf, size, pos);
						break;
				}
			} else {
				// unknown file type: using long header
				writeData(buf, size, pos);
			}
			firstFile = false;
		} else {
			// skipping unhandled data, shouldn't occur in normal cas file
			pos++;
			issueWarning = true;
		}
	}
	if (!headerFound) {
		throw MSXException(filename.getOriginal() +
		                   ": not a valid CAS image");
	}
	if (issueWarning) {
		 cliComm.printWarning("Skipped unhandled data in " +
		                      filename.getOriginal());
	}

	// conversion successful, now calc sha1sum
	setSha1Sum(filePool.getSha1Sum(file));
}

} // namespace openmsx
