// $Id$

#include "CasImage.hh"
#include "File.hh"
#include "CliComm.hh"
#include "Clock.hh"
#include "MSXException.hh"
#include <string.h> // for memcmp

using std::string;

namespace openmsx {

// output settings
const int FACTOR = 46; // 4.6 times normal speed (5520 baud, higher doesn't
                       // work anymore, possibly because the BIOS code (and Z80)
                       // is too slow for that)
const int OUTPUT_FREQUENCY = 2400 * FACTOR / 10;

// number of ouput bytes for silent parts
const int SHORT_SILENCE = OUTPUT_FREQUENCY * 1; // 1 second
const int LONG_SILENCE  = OUTPUT_FREQUENCY * 2; // 2 seconds

// number of 1-bits for headers
const int LONG_HEADER  = 16000 / 2;
const int SHORT_HEADER =  4000 / 2;

// headers definitions
const byte CAS_HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
const byte ASCII_HEADER[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
const byte BINARY_HEADER[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
const byte BASIC_HEADER[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };


CasImage::CasImage(const string& fileName, CliComm& cliComm_) 
	: cliComm(cliComm_)
{
	File file(fileName);
	size = file.getSize();
	buf = file.mmap();
	pos = 0;
	setFirstFileType(CassetteImage::UNKNOWN);
	convert(fileName);
}

CasImage::~CasImage()
{
}

short CasImage::getSampleAt(const EmuTime& time)
{
	static const Clock<OUTPUT_FREQUENCY> zero;
	unsigned pos = zero.getTicksTill(time);
	return pos < output.size() ? output[pos] * 256 : 0;
}

void CasImage::write0()
{
	output.push_back( 127);
	output.push_back( 127);
	output.push_back(-127);
	output.push_back(-127);
}
void CasImage::write1()
{
	output.push_back( 127);
	output.push_back(-127);
	output.push_back( 127);
	output.push_back(-127);
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
	output.insert(output.end(), s, 0);
}

// write a byte
void CasImage::writeByte(byte b)
{
	// one start bit
	write0();
	// eight data bits
	for (int i = 0; i < 8; ++i) {
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
bool CasImage::writeData()
{
	bool eof = false;
	while ((pos + 8) <= size) {
		if (!memcmp(&buf[pos], CAS_HEADER, 8)) {
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

void CasImage::convert(const std::string& fileName)
{
	// search for a header in the .cas file
	bool issueWarning = false;
	bool headerFound = false;
	bool firstFile = true;
	while ((pos + 8) <= size) {
		if (!memcmp(&buf[pos], CAS_HEADER, 8)) {
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
				if (!memcmp(&buf[pos], ASCII_HEADER, 10)) {
					type = CassetteImage::ASCII;
				} else if (!memcmp(&buf[pos], BINARY_HEADER, 10)) {
					type = CassetteImage::BINARY;
				} else if (!memcmp(&buf[pos], BASIC_HEADER, 10)) {
					type = CassetteImage::BASIC;
				}
				if (firstFile) setFirstFileType(type);
				switch (type) {
					case CassetteImage::ASCII:
						writeData();
						bool eof;
						do {
							pos += 8;
							writeSilence(SHORT_SILENCE);
							writeHeader(SHORT_HEADER);
							eof = writeData();
						} while (!eof && ((pos + 8) <= size));
						break;
					case CassetteImage::BINARY:
					case CassetteImage::BASIC:
						writeData();
						writeSilence(SHORT_SILENCE);
						writeHeader(SHORT_HEADER);
						pos += 8;
						writeData();
						break;
					default:
						// unknown file type: using long header
						writeData();
						break;
				}
			} else {
				// unknown file type: using long header
				writeData();
			}
			firstFile = false;
		} else {
			// should not occur
			PRT_DEBUG("CAS2WAV: skipping unhandled data");
			pos++;
			issueWarning = true;
		}
	}
	if (!headerFound) {
		string msg = fileName + ": not a valid CAS image";
		throw MSXException(msg);
	}
	if (issueWarning) {
		 cliComm.printWarning("Skipped unhandled data in " + fileName);
	}
}

} // namespace openmsx
