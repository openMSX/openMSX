// $Id$

#ifndef MSXKANJI12_HH
#define MSXKANJI12_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "MSXDeviceSwitch.hh"

namespace openmsx {

class MSXKanji12 : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXKanji12(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time);
	virtual ~MSXKanji12();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	Rom rom;
	int adr;
	int size;
};

} // namespace openmsx

#endif
