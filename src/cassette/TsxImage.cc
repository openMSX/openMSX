/*
	(2017) NataliaPC aka @ishwin74
	Under GPL License
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
    * [#11] Turbo Speed Block (Turbo speed)
    * [#12] Pure Tone Block
    * [#13] Pulse sequence Block
    * [#20] Silence Block
    * [#30] Text description Block
    * [#32] Archive info Block
    * [#35] Custom info Block
    * [#4B] MSX implementation of KCS (Kansas City Standard) Block
    * [#5A] Glue Block
*/
	static const uint8_t B10_STD_BLOCK      = 0x10;
	static const uint8_t B11_TURBO_BLOCK    = 0x11;
	static const uint8_t B12_PURE_TONE      = 0x12;
	static const uint8_t B13_PULSE_SEQUENCE = 0x13;
	static const uint8_t B20_SILENCE_BLOCK  = 0x20;
	static const uint8_t B30_TEXT_DESCRIP   = 0x30;
	static const uint8_t B32_ARCHIVE_INFO   = 0x32;
	static const uint8_t B35_CUSTOM_INFO    = 0x35;
	static const uint8_t B4B_MSX_KCS        = 0x4B;
	static const uint8_t B5A_GLUE_BLOCK     = 0x5A;
/*
    Blocks detected and skipped but not yet supported or not very useful
    for MSX tapes:

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
	static const uint8_t B14_PURE_DATA      = 0x14;
	static const uint8_t B15_DIRECT_REC     = 0x15;
	static const uint8_t B19_GEN_DATA       = 0x19;
	static const uint8_t B21_GRP_START      = 0x21;
	static const uint8_t B22_GRP_END        = 0x22;
	static const uint8_t B23_JUMP_BLOCK     = 0x23;
	static const uint8_t B24_LOOP_START     = 0x24;
	static const uint8_t B25_LOOP_END       = 0x25;
	static const uint8_t B26_CALL_SEQ       = 0x26;
	static const uint8_t B27_RET_SEQ        = 0x27;
	static const uint8_t B31_MSG_BLOCK      = 0x31;

/*
	Standard MSX Blocks (#4B) can be forced to 3600 bauds, reduction of pilot
	pulses, and minimal pauses after blocks (~100ms).
	The Turbo blocks (#10) can't be forced beyond 140% over real speed but they
	aren't forced at all to maintain compatibility with all Turbo loaders.
*/
static const bool     ULTRA_SPEED       = false;    // 3600 bauds / short pilots / minimal block pauses

// output settings
static const unsigned TZX_Z80_FREQ      = 3500000;  // 3.5 Mhz
static const unsigned OUTPUT_FREQ       = 58900;    // ~ = Z80_FREQ*4/TSTATES_MSX_PULSE
static const float    TSTATES_MSX_PULSE = 238.f;

// headers definitions
static const byte TSX_HEADER   [ 8] = { 'Z','X','T','a','p','e','!', 0x1a};
static const byte ASCII_HEADER[10]  = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
static const byte BINARY_HEADER[10] = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
static const byte BASIC_HEADER[10]  = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

inline uint16_t tstates2bytes(uint32_t tstates)
{
	return 	( tstates * OUTPUT_FREQ / TZX_Z80_FREQ );
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

void TsxImage::writePulse(uint32_t tstates)
{
	acumBytes += tstates2bytes(tstates);
	output.insert(end(output), int(acumBytes), currentValue);
	acumBytes -= int(acumBytes);
	currentValue = -currentValue;
}

void TsxImage::write0()
{
	writePulse(pulseZero4B);
	writePulse(pulseZero4B);
}
void TsxImage::write1()
{
	writePulse(pulseOne4B);
	writePulse(pulseOne4B);
	writePulse(pulseOne4B);
	writePulse(pulseOne4B);
}

// write a header signal
void TsxImage::writeHeader4B(int s)
{
	for (int i = 0; i < s; ++i) {
		writePulse(pulsePilot4B);
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

size_t TsxImage::writeBlock10(Block10 *b)   //Standard Speed Block
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

size_t TsxImage::writeBlock11(Block11 *b)   //Turbo Speed Block
{
	currentValue = -127;
	for (int i=0; i<b->pilotlen; i++) {
		writeTurboPilot(b->pilot);
	}
	writeTurboSync(b->sync1, b->sync2);
	
	uint32_t size = b->len;
	byte *data = b->data;
	while (size--) {
		writeTurboByte(*data++, b->zero, b->one);
	}
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return b->len + sizeof(Block11);
}

size_t TsxImage::writeBlock12(Block12 *b)   //Pure Tone Block
{
	uint32_t pulse = tstates2bytes(b->len);
	for (int i = 0; i < b->pulses/2; ++i) {
		writePulse(pulse);
		writePulse(pulse);
	}
	return sizeof(Block12);
}

size_t TsxImage::writeBlock13(Block13 *b)   //Pulse sequence Block
{
	for (int i = 0; i < b->num; ++i) {
		writePulse(tstates2bytes(b->pulses[i]));
	}
	return b->num*2 + 2;
}

size_t TsxImage::writeBlock20(Block20 *b)   //Silence Block
{
	writeSilence(ULTRA_SPEED ? 100 : b->pausems);
	return sizeof(Block20);
}

size_t TsxImage::writeBlock30(Block30 *b, CliComm& cliComm) //Text description Block
{
	char text[256];
	text[b->len] = '\0';
	memcpy(text, &b->text, b->len);
	cliComm.printInfo(text);
	return b->len + 2;
}

size_t TsxImage::writeBlock32(Block32 *b, CliComm& cliComm) //Archive info Block
{
	byte num = b->num;
	byte *list = b->list;
	while (num--) {
		if (list[0]==0x00) {
			char text[256];
			text[list[1]] = '\0';
			memcpy(text, &list[2], list[1]);
			cliComm.printInfo(text);
			break;
		}
		list += 1 + list[1];
	}
	return b->blockLen + 3;
}

size_t TsxImage::writeBlock35(Block35 *b)   //Custom info Block
{
	return b->len + 21;
}

size_t TsxImage::writeBlock4B(Block4B *b, CliComm& cliComm) //MSX KCS Block
{
	pulsePilot4B = ULTRA_SPEED ? TSTATES_MSX_PULSE : b->pilot;
	pulseOne4B   = ULTRA_SPEED ? TSTATES_MSX_PULSE : b->bit1len;
	pulseZero4B  = ULTRA_SPEED ? TSTATES_MSX_PULSE*2 : b->bit0len;

	if (b->bitcfg!=MSX_BITCFG || b->bytecfg!=MSX_BYTECFG) {
		cliComm.printWarning("Found bad standard MSX block. Assuming default values. Please check your TSX file...");
	}

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
	uint8_t bid = 0;       //BlockId
	size_t pos = 0;

	if (!memcmp(&buf[pos], TSX_HEADER, 8)) {
		headerFound = true;
		pos += 10;         //Skip TZX header (8 bytes) + major/minor version (2 bytes)
		while (pos < size) {
			acumBytes = 0.f;
			bid = buf[pos];
			if (bid == B10_STD_BLOCK) {
#ifdef DEBUG
				cliComm.printInfo("Block#10");
#endif
				pos += writeBlock10((Block10*)&buf[pos]);
			} else
			if (bid == B11_TURBO_BLOCK) {
#ifdef DEBUG
				cliComm.printInfo("Block#11");
#endif
				pos += writeBlock11((Block11*)&buf[pos]);
			} else
			if (bid == B12_PURE_TONE) {
#ifdef DEBUG
				cliComm.printInfo("Block#12");
#endif
				pos += writeBlock12((Block12*)&buf[pos]);
			} else
			if (bid == B13_PULSE_SEQUENCE) {
#ifdef DEBUG
				cliComm.printInfo("Block#13");
#endif
				pos += writeBlock13((Block13*)&buf[pos]);
			} else
			if (bid == B20_SILENCE_BLOCK) {
#ifdef DEBUG
				cliComm.printInfo("Block#20");
#endif
				pos += writeBlock20((Block20*)&buf[pos]);
			} else
			if (bid == B30_TEXT_DESCRIP) {
#ifdef DEBUG
				cliComm.printInfo("Block#30");
#endif
				pos += writeBlock30((Block30*)&buf[pos], cliComm);
			} else
			if (bid == B32_ARCHIVE_INFO) {
#ifdef DEBUG
				cliComm.printInfo("Block#32");
#endif
				pos += writeBlock32((Block32*)&buf[pos], cliComm);
			} else
			if (bid == B35_CUSTOM_INFO) {
#ifdef DEBUG
				cliComm.printInfo("Block#35");
#endif
				pos += writeBlock35((Block35*)&buf[pos]);
			} else
			if (bid == B4B_MSX_KCS) {
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
				pos += writeBlock4B(b, cliComm);
			} else
			if (bid == B5A_GLUE_BLOCK) {
#ifdef DEBUG
				cliComm.printInfo("Block#5A");
#endif
				pos += 10;
			} else
			if (bid == B14_PURE_DATA) {
				cliComm.printWarning("Block#14 Unsupported yet!");
				pos += *((UA_L24*)&buf[pos+8]) + 11;
			} else
			if (bid == B15_DIRECT_REC) {
				cliComm.printWarning("Block#15 Unsupported yet!");
				pos += *((UA_L24*)&buf[pos+6]) + 9;
			} else
			if (bid == B19_GEN_DATA) {
				cliComm.printWarning("Block#19 Unsupported yet!");
				pos += *((uint32_t*)&buf[pos+1]) + 5;
			} else
			if (bid == B21_GRP_START) {
				cliComm.printWarning("Block#21 Unsupported yet!");
				pos += *((uint8_t*)&buf[pos+1]) + 2;
			} else
			if (bid == B22_GRP_END) {
				cliComm.printWarning("Block#22 Unsupported yet!");
				pos += 1;
			} else
			if (bid == B23_JUMP_BLOCK) {
				cliComm.printWarning("Block#23 Unsupported yet!");
				pos += 3;
			} else
			if (bid == B24_LOOP_START) {
				cliComm.printWarning("Block#24 Unsupported yet!");
				pos += 3;
			} else
			if (bid == B25_LOOP_END) {
				cliComm.printWarning("Block#25 Unsupported yet!");
				pos += 1;
			} else
			if (bid == B26_CALL_SEQ) {
				cliComm.printWarning("Block#26 Unsupported yet!");
				pos += *((uint16_t*)&buf[pos+1])*2 + 3;
			} else
			if (bid == B27_RET_SEQ) {
				cliComm.printWarning("Block#27 Unsupported yet!");
				pos += 1;
			} else
			if (bid == B31_MSG_BLOCK) {
				cliComm.printWarning("Block#31 Unsupported yet!");
				pos += *((uint8_t*)&buf[pos+2]) + 3;
			} else {
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
