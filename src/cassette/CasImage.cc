// $Id$

/**
 * Code based on "cas2wav" tool by Vincent van Dam
 */

#include "CasImage.hh"
#include "File.hh"
#include "EmuTime.hh"
#include <cstdlib>
#include <memory.h>


namespace openmsx {

// output settings
const int OUTPUT_FREQUENCY = 43200;

// number of ouput bytes for silent parts
const int SHORT_SILENCE = OUTPUT_FREQUENCY;	// 1 second
const int LONG_SILENCE  = OUTPUT_FREQUENCY * 2;	// 2 seconds

// frequency for pulses
const int LONG_PULSE  = 1200;
const int SHORT_PULSE = 2400;

// number of short pulses for headers
const int LONG_HEADER  = 16000;
const int SHORT_HEADER = 4000;

// headers definitions
const byte HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
const byte ASCII[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
const byte BIN[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
const byte BASIC[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };


CasImage::CasImage(const string& fileName)
{
	File file(fileName);
	size = file.getSize();
	buf = file.mmap();
	pos = 0;
	baudRate = 2400;
	convert();
	file.munmap();
}

CasImage::~CasImage()
{
}

short CasImage::getSampleAt(const EmuTime& time)
{
	unsigned pos = time.getTicksAt(OUTPUT_FREQUENCY);
	if (pos < output.size()) {
		return output[pos] * 256;
	} else {
		return 0;
	}
}


// write a pulse
void CasImage::writePulse(int f)
{
	int length = OUTPUT_FREQUENCY / (baudRate * (f / 1200));

	int i = 0;
	for ( ; i < (length / 2); i++) {
		output.push_back(127);
	}
	for ( ; i < length; i++) {
		output.push_back(-127);
	}
}

// write a header signal
void CasImage::writeHeader(int s)
{
	for (int i = 0; i < s * (baudRate / 1200); i++) {
		writePulse(SHORT_PULSE);
	}
}

// write silence
void CasImage::writeSilence(int s)
{
	output.insert(output.end(), s, 0);
}

// write a byte
void CasImage::writeByte(byte b)
{
	// one start bit
	writePulse(LONG_PULSE);

	// eight data bits
	for (int i = 0; i < 8; i++) {
		if (b & 1) {
			writePulse(SHORT_PULSE);
			writePulse(SHORT_PULSE);
		} else {
			writePulse(LONG_PULSE);
		}
		b = b >> 1;
	}

	// two stop bits
	for (int i = 0; i < 4; i++) {
		writePulse(SHORT_PULSE);
	}
}

// write data until a header is detected
bool CasImage::writeData()
{
	bool eof = false;
	while ((pos + 8) <= size) {
		if (!memcmp(&buf[pos], HEADER, 8)) {
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

void CasImage::convert()
{
	// search for a header in the .cas file
	while ((pos + 8) <= size) {
		if (!memcmp(&buf[pos], HEADER, 8)) {
			// it probably works fine if a long header is used for every
			// header but since the msx bios makes a distinction between
			// them, we do also (hence a lot of code).
			pos += 8;
			writeSilence(LONG_SILENCE);
			writeHeader(LONG_HEADER);
			if ((pos + 10) <= size) {
				if (!memcmp(&buf[pos], ASCII, 10)) {
					writeData();
					bool eof;
					do {
						pos += 8;
						writeSilence(SHORT_SILENCE);
						writeHeader(SHORT_HEADER);
						eof = writeData();
					} while (!eof && ((pos + 8) <= size));
				} else if (!memcmp(&buf[pos], BIN, 10) ||
					   !memcmp(&buf[pos], BASIC, 10)) {
					writeData();
					writeSilence(SHORT_SILENCE);
					writeHeader(SHORT_HEADER);
					pos += 8;
					writeData();
				} else {
					// unknown file type: using long header
					writeData();
				}
			} else { 
				// unknown file type: using long header
				writeData();
			}
		} else {
			// should not occur
			PRT_DEBUG("CAS2WAV: skipping unhandled data");
			pos++;
		}
	}
}

} // namespace openmsx
