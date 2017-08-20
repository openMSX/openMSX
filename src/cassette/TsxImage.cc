/*
	Copyleft 2017
	NataliaPC aka @ishwin74
*/
#include "TsxImage.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "Clock.hh"
#include "MSXException.hh"
#include "xrange.hh"
#include <cstring> // for memcmp/memcpy
#include <iostream>

namespace openmsx {

/*
    Current version supports and read the next TZX 1.20 blocks 
    (and #4B TSX blocks):

    * [#10] Standard Speed Block (Turbo normal)
    * [#12] Pure Tone Block
    * [#13] Pulse sequence Block
    * [#20] Silence Block
    * [#30] Text description Block
    * [#32] Archive info Block
    * [#35] Custom info Block
    * [#4B] MSX implementation of KCS (Kansas City Standard) Block
    * [#5A] Glue Block

    Blocks detected and skipped but not yet supported or not very useful
    for MSX tapes:

    * [#11] Turbo Speed Block (Turbo speed)
    * [#14] Pure data Block
    * [#15] Direct recording Block
    * [#19] Generalized data block
    * [#21] Group start Block
    * [#22] Group end Block
    * [#23] Jump Block
    * [#24] Loop start Block
    * [#25] Loop end Block
    * [#26] Call sequence Block
    * [#27] Return sequence Block
    * [#31] Message Block
*/

/*
	Standard MSX Blocks (#4B) can be forced to 3600 bauds, reduction of pilot
	pulses, and after minimal block pauses.
	The Turbo blocks (#10) can't be forced beyond 140% over real speed but they
	are not forced to maintain compatibility with all Turbo loaders.
*/
static const bool     ULTRA_SPEED       = true;		//3600 bauds / short pilots / minimal block pauses

// output settings
static const unsigned Z80_FREQ          = 3500000;	// 3.5 Mhz
static const unsigned OUTPUT_FREQ       = 58900;	// ~ = Z80_FREQ*4/TSTATES_MSX_PULSE
static const float    TSTATES_MSX_PULSE = 238.f;

// headers definitions
static const byte TSX_HEADER   [ 8] = { 'Z','X','T','a','p','e','!', 0x1a};
static const byte ASCII_HEADER[10]  = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
static const byte BINARY_HEADER[10] = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
static const byte BASIC_HEADER[10]  = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

#define tstates2bytes(tstates)	( (tstates) * OUTPUT_FREQ / Z80_FREQ )


inline uint16_t toLe16(byte *data)
{
	return (((uint16_t)data[1])<<0) | (((uint16_t)data[0])<<8);
}
inline uint24_t toLe24(byte *data)
{
	return (((uint24_t)data[2])<<0) | (((uint24_t)data[1])<<8) | (((uint24_t)data[0])<<16);
}
inline uint32_t toLe32(byte *data)
{
	return (((uint32_t)data[3])<<0) | (((uint32_t)data[2])<<8) | (((uint32_t)data[1])<<16) | (((uint32_t)data[0])<<24);
}


TsxImage::TsxImage(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	setFirstFileType(CassetteImage::UNKNOWN);
	convert(filename, filePool, cliComm);
}

int16_t TsxImage::getSampleAt(EmuTime::param time)
{
	static const Clock<OUTPUT_FREQ> zero(EmuTime::zero);
	unsigned pos = zero.getTicksTill(time);
	return pos < output.size() ? output[pos] * 256 : 0;
}

EmuTime TsxImage::getEndTime() const
{
	Clock<OUTPUT_FREQ> clk(EmuTime::zero);
	clk += unsigned(output.size());
	return clk.getTime();
}

unsigned TsxImage::getFrequency() const
{
	return OUTPUT_FREQ;
}

void TsxImage::fillBuffer(unsigned pos, int** bufs, unsigned num) const
{
	size_t nbSamples = output.size();
	if (pos < nbSamples) {
		for (auto i : xrange(num)) {
			bufs[0][i] = (pos < nbSamples)
			           ? output[pos] * 256
			           : 0;
			++pos;
		}
	} else {
		bufs[0] = nullptr;
	}
}

void TsxImage::writePulse(uint32_t tstates) {
	acumBytes += tstates2bytes(tstates);
	output.insert(end(output), int(acumBytes), currentValue);
	acumBytes -= int(acumBytes);
	currentValue = -currentValue;
}

void TsxImage::write0()
{
	writePulse(pulse4B*2);
	writePulse(pulse4B*2);
}
void TsxImage::write1()
{
	writePulse(pulse4B);
	writePulse(pulse4B);
	writePulse(pulse4B);
	writePulse(pulse4B);
}

// write a header signal
void TsxImage::writeHeader4B(int s)
{
	for (int i = 0; i < s; ++i) {
		writePulse(pulse4B);
	}
}

// write a MSX #4B byte
void TsxImage::writeByte4B(byte b)
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

// write silence
void TsxImage::writeSilence(int ms)
{
	if (ms) {
		output.insert(end(output), OUTPUT_FREQ*ms/1000, 0);
		currentValue = 127;
	}
}

void TsxImage::writeTurboPilot(uint16_t tstates=2168)
{
	writePulse(tstates);
}

void TsxImage::writeTurboSync(uint16_t sync1=667, uint16_t sync2=735)
{
	writePulse(sync1);
	writePulse(sync2);
}

void TsxImage::writeTurbo0(uint16_t tstates=855)
{
	writePulse(tstates);
	writePulse(tstates);
}

void TsxImage::writeTurbo1(uint16_t tstates=1710)
{
	writePulse(tstates);
	writePulse(tstates);
}

// write a turbo #10 byte
void TsxImage::writeTurboByte(byte b, uint16_t zerolen=855, uint16_t onelen=1710)
{
	// eight data bits
	for (auto i : xrange(8)) {
		if (b & (128 >> i)) {
			writeTurbo1(onelen);
		} else {
			writeTurbo0(zerolen);
		}
	}
}

size_t TsxImage::writeBlock10(Block10 *b)	//Standard Speed Block
{
	currentValue = -127;
	for (int i=0; i<3223; i++) {
		writeTurboPilot();
	}
	writeTurboSync();
	
	uint16_t size = b->len;
	byte *data = b->data;
	while (size--) {
		writeTurboByte(*data++);
	}
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return b->len + 5;
}

size_t TsxImage::writeBlock11(Block11 *b)	//Turbo Speed Block
{
	currentValue = -127;
	for (int i=0; i<b->pilotlen; i++) {
		writeTurboPilot(b->pilot);
	}
	writeTurboSync(b->sync1, b->sync2);
	
	uint32_t size = (uint24_t)b->len;
	byte *data = b->data;
	while (size--) {
		writeTurboByte(*data++, b->zero, b->one);
	}
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return ((uint24_t)b->len) + sizeof(Block11);
}

size_t TsxImage::writeBlock12(Block12 *b)	//Pure Tone Block
{
	uint32_t pulse = tstates2bytes(b->len);
	for (int i = 0; i < b->pulses/2; ++i) {
		writePulse(pulse);
		writePulse(pulse);
	}
	return sizeof(Block12);
}

size_t TsxImage::writeBlock13(Block13 *b)	//Pulse sequence Block
{
	for (int i = 0; i < b->num; ++i) {
		writePulse(tstates2bytes(b->pulses[i]));
	}
	return b->num*2 + 2;
}

size_t TsxImage::writeBlock20(Block20 *b)	//Silence Block
{
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return sizeof(Block20);
}

size_t TsxImage::writeBlock30(Block30 *b, CliComm& cliComm)	//Text description Block
{
	char text[b->len + 1];
	text[b->len] = '\0';
	memcpy(text, &b->text, b->len);
	cliComm.printInfo(text);
	return b->len + 2;
}

size_t TsxImage::writeBlock32(Block32 *b, CliComm& cliComm)	//Archive info Block
{
	byte num = b->num;
	byte *list = b->list;
	while (num--) {
		if (list[0]==0x00) {
			char text[list[1]+1];
			text[list[1]] = '\0';
			memcpy(text, &list[2], list[1]);
			cliComm.printInfo(text);
			break;
		}
		list += 1 + list[1];
	}
	return b->blockLen + 3;
}

size_t TsxImage::writeBlock35(Block35 *b)	//Custom info Block
{
	return b->len + 21;
}

size_t TsxImage::writeBlock4B(Block4B *b)	//MSX KCS Block
{
	pulse4B = ULTRA_SPEED ? TSTATES_MSX_PULSE : b->pilot;

	writeHeader4B(ULTRA_SPEED ? 5000 : b->pulses);
	
	uint32_t size = b->blockLen - 12;
	byte *data = b->data;
	while (size--) {
		writeByte4B(*data++);
	}
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return b->blockLen + 5;
}

void TsxImage::convert(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	File file(filename);
	size_t size;
	const byte* buf = file.mmap(size);

	// search for a header in the .tsx file
	bool issueWarning = false;
	bool headerFound = false;
	bool firstFile = true;
	uint8_t bid = 0;	//BlockId
	size_t pos = 0;

	if (!memcmp(&buf[pos], TSX_HEADER, 8)) {
		headerFound = true;
		pos += 10;	//Skip TZX header (8 bytes) + major/minor version (2 bytes)
		while (pos < size) {
			acumBytes = 0.f;
			bid = buf[pos];
			if (bid == 0x10) {	//Standard Speed Block (Turbo)
#ifdef DEBUG
				cliComm.printInfo("Block#10");
#endif
				pos += writeBlock10((Block10*)&buf[pos]);
			} else
			if (bid == 0x11) {	//Turbo Speed Block (Turbo)
#ifdef DEBUG
				cliComm.printInfo("Block#11");
#endif
				pos += writeBlock11((Block11*)&buf[pos]);
			} else
			if (bid == 0x12) {	//Pure Tone Block
#ifdef DEBUG
				cliComm.printInfo("Block#12");
#endif
				pos += writeBlock12((Block12*)&buf[pos]);
			} else
			if (bid == 0x13) {	//Pulse sequence Block
#ifdef DEBUG
				cliComm.printInfo("Block#13");
#endif
				pos += writeBlock13((Block13*)&buf[pos]);
			} else
			if (bid == 0x20) {	//Silence Block
#ifdef DEBUG
				cliComm.printInfo("Block#20");
#endif
				pos += writeBlock20((Block20*)&buf[pos]);
			} else
			if (bid == 0x30) {	//Text description Block
#ifdef DEBUG
				cliComm.printInfo("Block#30");
#endif
				pos += writeBlock30((Block30*)&buf[pos], cliComm);
			} else
			if (bid == 0x32) {	//Archive info Block
#ifdef DEBUG
				cliComm.printInfo("Block#32");
#endif
				pos += writeBlock32((Block32*)&buf[pos], cliComm);
			} else
			if (bid == 0x35) {	//Custom info Block
#ifdef DEBUG
				cliComm.printInfo("Block#35");
#endif
				pos += writeBlock35((Block35*)&buf[pos]);
			} else
			if (bid == 0x4B) {	//MSX KCS Block
				Block4B *b = (Block4B*)&buf[pos];
#ifdef DEBUG
				cliComm.printInfo("Block#4B");
#endif
				//determine file type
				if (firstFile && (pos+12+5)<size && b->blockLen-12==16) {
					FileType type = CassetteImage::UNKNOWN;
					if (!memcmp(&(b->data), ASCII_HEADER, 10)) {
						type = CassetteImage::ASCII;
					} else if (!memcmp(&(b->data), BINARY_HEADER, 10)) {
						type = CassetteImage::BINARY;
					} else if (!memcmp(&(b->data), BASIC_HEADER, 10)) {
						type = CassetteImage::BASIC;
					}
					setFirstFileType(type);
					firstFile = false;
				}
				//read the block
				pos += writeBlock4B(b);
			} else
			if (bid == 0x5A) {	//Glue Block
#ifdef DEBUG
				cliComm.printInfo("Block#5A");
#endif
				pos += 10;
			} else
			if (bid == 0x14) {	//Pure data Block
				cliComm.printWarning("Block#14 Unsupported yet!");
				pos += *((uint24_t*)&buf[pos+8]) + 11;
			} else
			if (bid == 0x15) {	//Direct recording Block
				cliComm.printWarning("Block#15 Unsupported yet!");
				pos += *((uint24_t*)&buf[pos+6]) + 9;
			} else
			if (bid == 0x19) {	//Generalized data block
				cliComm.printWarning("Block#19 Unsupported yet!");
				pos += *((uint32_t*)&buf[pos+1]) + 5;
			} else
			if (bid == 0x21) {	//Group start Block
				cliComm.printWarning("Block#21 Unsupported yet!");
				pos += *((uint8_t*)&buf[pos+1]) + 1;
			} else
			if (bid == 0x22) {	//Group end Block
				cliComm.printWarning("Block#22 Unsupported yet!");
				pos += 1;
			} else
			if (bid == 0x23) {	//Jump Block
				cliComm.printWarning("Block#23 Unsupported yet!");
				pos += 3;
			} else
			if (bid == 0x24) {	//Loop start Block
				cliComm.printWarning("Block#24 Unsupported yet!");
				pos += 3;
			} else
			if (bid == 0x25) {	//Loop end Block
				cliComm.printWarning("Block#25 Unsupported yet!");
				pos += 1;
			} else
			if (bid == 0x26) {	//Call sequence Block
				cliComm.printWarning("Block#26 Unsupported yet!");
				pos += *((uint16_t*)&buf[pos+1])*2 + 3;
			} else
			if (bid == 0x27) {	//Return sequence Block
				cliComm.printWarning("Block#27 Unsupported yet!");
				pos += 1;
			} else
			if (bid == 0x31) {	//Message Block
				cliComm.printWarning("Block#31 Unsupported yet!");
				pos += *((uint8_t*)&buf[pos+2]) + 3;
			} else {		// Not supported yet Block type
				// skipping unhandled data, shouldn't occur in normal tsx file
				char buff[256];
				sprintf(buff, "Unknown TSX block #%02x", bid);
				cliComm.printWarning(buff);
				pos++;
				issueWarning = true;
			}
		}
	}
	if (!headerFound) {
		throw MSXException(filename.getOriginal() +
		                   ": not a valid TSX image");
	}
	if (issueWarning) {
		 cliComm.printWarning("Skipped unhandled data in " +
		                      filename.getOriginal());
	}

	// conversion successful, now calc sha1sum
	setSha1Sum(filePool.getSha1Sum(file));
}

} // namespace openmsx
