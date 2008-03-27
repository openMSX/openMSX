// $Id$

#ifndef MSXMATSUSHITA_HH
#define MSXMATSUSHITA_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
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

	// MSXDevice
	virtual void reset(const EmuTime& time);

	// MSXSwitchedDevice
	virtual byte readSwitchedIO(word port, const EmuTime& time);
	virtual byte peekSwitchedIO(word port, const EmuTime& time) const;
	virtual void writeSwitchedIO(word port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
	const std::auto_ptr<SRAM> sram;
	word address;

	nibble color1, color2;
	byte pattern;
};

} // namespace openmsx

#endif
