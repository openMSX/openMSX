// $Id$

#ifndef Y8950_HH
#define Y8950_HH

#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class Y8950Periphery;
class EmuTime;
class Y8950Impl;

class Y8950
{
public:
	static const int CLOCK_FREQ = 3579545;

	// Bitmask for register 0x04
	// Timer1 Start.
	static const int R04_ST1          = 0x01;
	// Timer2 Start.
	static const int R04_ST2          = 0x02;
	// not used
	//static const int R04            = 0x04;
	// Mask 'Buffer Ready'.
	static const int R04_MASK_BUF_RDY = 0x08;
	// Mask 'End of sequence'.
	static const int R04_MASK_EOS     = 0x10;
	// Mask Timer2 flag.
	static const int R04_MASK_T2      = 0x20;
	// Mask Timer1 flag.
	static const int R04_MASK_T1      = 0x40;
	// IRQ RESET.
	static const int R04_IRQ_RESET    = 0x80;

	// Bitmask for status register
	static const int STATUS_EOS     = R04_MASK_EOS;
	static const int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
	static const int STATUS_T2      = R04_MASK_T2;
	static const int STATUS_T1      = R04_MASK_T1;

	Y8950(MSXMotherBoard& motherBoard, const std::string& name,
	      const XMLElement& config, unsigned sampleRam, const EmuTime& time,
	      Y8950Periphery& perihery);
	~Y8950();

	void setEnabled(bool enabled, const EmuTime& time);
	void reset(const EmuTime& time);
	void writeReg(byte reg, byte data, const EmuTime& time);
	byte readReg(byte reg, const EmuTime& time);
	byte peekReg(byte reg, const EmuTime& time) const;
	byte readStatus();
	byte peekStatus() const;

	// for ADPCM
	void setStatus(byte flags);
	void resetStatus(byte flags);

private:
	const std::auto_ptr<Y8950Impl> pimple;
};

} // namespace openmsx

#endif
