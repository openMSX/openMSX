// $Id$

/*
 * This class implements the
 *   backup RAM
 *   bitmap function
 * of the S1985 MSX-engine
 * 
 *  TODO explanation  
 */

#ifndef __S1985_HH__
#define __S1985_HH__

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"

namespace openmsx {

class MSXS1985 : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXS1985(Config* config, const EmuTime& time);
	virtual ~MSXS1985();
	
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	nibble address;
	byte ram[0x10];

	byte color1;
	byte color2;
	byte pattern;
};

} // namespace openmsx

#endif
