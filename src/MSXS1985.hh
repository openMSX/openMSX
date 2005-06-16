// $Id$

/*
 * This class implements the
 *   backup RAM
 *   bitmap function
 * of the S1985 MSX-engine
 *
 *  TODO explanation
 */

#ifndef S1985_HH
#define S1985_HH

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include <memory>

namespace openmsx {

class Ram;

class MSXS1985 : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXS1985(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time);
	virtual ~MSXS1985();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<Ram> ram;
	nibble address;

	byte color1;
	byte color2;
	byte pattern;
};

} // namespace openmsx

#endif
