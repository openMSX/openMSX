// $Id$

/*
 * This class implements the device found on IO-port 0xF4 on a MSX Turbo-R.
 * 
 *  TODO explanation  
 */

#ifndef __F4DEVICE_HH__
#define __F4DEVICE_HH__

#include "MSXDevice.hh"

namespace openmsx {

class MSXF4Device : public MSXDevice
{
public:
	MSXF4Device(const XMLElement& config, const EmuTime& time);
	virtual ~MSXF4Device();
	
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	bool inverted;
	byte status;
};

} // namespace openmsx

#endif
