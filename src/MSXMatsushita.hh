// $Id$

#ifndef __MSXMATSUSHITA_HH__
#define __MSXMATSUSHITA_HH__

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "FirmwareSwitch.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXMatsushita : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXMatsushita(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMatsushita();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	FirmwareSwitch firmwareSwitch;
	SRAM sram;
	word address;

	nibble color1, color2;
	byte pattern;
};

} // namespace openmsx

#endif // __MSXMATSUSHITA_HH__
