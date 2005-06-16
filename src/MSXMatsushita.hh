// $Id$

#ifndef MSXMATSUSHITA_HH
#define MSXMATSUSHITA_HH

#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include <memory>

namespace openmsx {

class FirmwareSwitch;
class SRAM;

class MSXMatsushita : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXMatsushita(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time);
	virtual ~MSXMatsushita();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
	const std::auto_ptr<SRAM> sram;
	word address;

	nibble color1, color2;
	byte pattern;
};

} // namespace openmsx

#endif
