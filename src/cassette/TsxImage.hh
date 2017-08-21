/*
	(2017) NataliaPC aka @ishwin74
	Under GPL License
*/
#ifndef TSXIMAGE_HH
#define TSXIMAGE_HH

#include "CassetteImage.hh"
#include "openmsx.hh"
#include "endian.hh"
#include <vector>


using namespace Endian;


namespace openmsx {

class CliComm;
class Filename;
class FilePool;

/**
 * Code based on "CasImage" class
 */
class TsxImage final : public CassetteImage
{
public:
	TsxImage(const Filename& fileName, FilePool& filePool, CliComm& cliComm);

	// CassetteImage
	int16_t getSampleAt(EmuTime::param time) override;
	EmuTime getEndTime() const override;
	unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, int** bufs, unsigned num) const override;

private:
	const static uint8_t MSX_BITCFG  = 0x24;
	const static uint8_t MSX_BYTECFG = 0x54;

	struct Block10 {
		uint8_t  id;
		UA_L16   pausems;           //Pause after this block in milliseconds
		UA_L16   len;               //Length of data that follow
		byte     data[0];           //Data as in .TAP files
	};
	struct Block11 {
		uint8_t  id;
		UA_L16   pilot;             //Length of PILOT pulse {2168}
		UA_L16   sync1;             //Length of SYNC first pulse {667}
		UA_L16   sync2;             //Length of SYNC second pulse {735}
		UA_L16   zero;              //Length of ZERO bit pulse {855}
		UA_L16   one;               //Length of ONE bit pulse {1710}
		UA_L16   pilotlen;          //Length of PILOT tone (number of pulses) {8063 header (flag<128), 3223 data (flag>=128)}
		uint8_t  lastbyte;          //Used bits in the last byte (other bits should be 0) {8}
		UA_L16   pausems;           //Pause after this block (ms.) {1000}
		UA_L24   len;               //Length of data that follow
		byte	 data[0];           //Data as in .TAP files
	};
	struct Block12 {
		uint8_t  id;
		UA_L16   len;               //Length of one pulse in T-states
		UA_L16   pulses;            //Number of pulses
	};
	struct Block13 {
		uint8_t  id;
		uint8_t  num;               //Number of pulses
		UA_L16   pulses[0];         //[Array] Pulses' lengths
	};
	struct Block20 {
		uint8_t  id;
		UA_L16   pausems;           //Silence pause in milliseconds
	};
	struct Block30 {
		uint8_t  id;
		uint8_t  len;               //Length of the text description
		char     text[0];           //[Array] Text description in ASCII format
	};
	struct Block32 {
		uint8_t  id;
		UA_L16   blockLen;          //Length of the whole block (without these two bytes)
		uint8_t  num;               //Number of text strings
		byte     list[0];           //[Array] List of text strings
	};
	struct Block35 {
		uint8_t  id;
		char     label[0x10];       //Identification string (in ASCII)
		UA_L32   len;               //Length of the custom info
		byte     data[0];           //[Array] Custom info
	};
	struct Block4B {
		uint8_t  id;
		UA_L32   blockLen;          //Block length without these four bytes
		UA_L16   pausems;           //Pause after this block in milliseconds
		UA_L16   pilot;             //Duration of a PILOT pulse in T-states {same as ONE pulse}
		UA_L16   pulses;            //Number of pulses in the PILOT tone
		UA_L16   bit0len;           //Duration of a ZERO pulse in T-states {=2*pilot}
		UA_L16   bit1len;           //Duration of a ONE pulse in T-states {=pilot}
		uint8_t  bitcfg = MSX_BITCFG;
		uint8_t  bytecfg = MSX_BYTECFG;
		byte     data[0];           //[Array]
	};

	size_t writeBlock10(Block10 *);
	size_t writeBlock11(Block11 *);
	size_t writeBlock12(Block12 *);
	size_t writeBlock13(Block13 *);
	size_t writeBlock20(Block20 *);
	size_t writeBlock30(Block30 *, CliComm& cliComm);
	size_t writeBlock32(Block32 *, CliComm& cliComm);
	size_t writeBlock35(Block35 *);
	size_t writeBlock4B(Block4B *, CliComm& cliComm);
	void writePulse(uint32_t tstates);
	void writeSilence(int s);
	void write0();
	void write1();
	void writeHeader4B(int s);
	void writeByte4B(byte b);
	void writeTurboPilot(uint16_t tstates);
	void writeTurboSync(uint16_t sync1, uint16_t sync2);
	void writeTurbo0(uint16_t tstates);
	void writeTurbo1(uint16_t tstates);
	void writeTurboByte(byte b, uint16_t zerolen, uint16_t onelen);

	void convert(const Filename& filename, FilePool& filePool, CliComm& cliComm);

	int8_t   currentValue = 127;
	uint32_t pulsePilot4B = 0;
	uint32_t pulseOne4B = 0;
	uint32_t pulseZero4B = 0;
	float    acumBytes = 0.f;

	std::vector<signed char> output;

};

} // namespace openmsx

#endif
