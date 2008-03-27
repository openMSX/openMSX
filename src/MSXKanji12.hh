// $Id$

#ifndef MSXKANJI12_HH
#define MSXKANJI12_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXKanji12 : public MSXDevice, public MSXSwitchedDevice
{
public:
	MSXKanji12(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time);
	virtual ~MSXKanji12();

	// MSXDevice
	virtual void reset(const EmuTime& time);

	// MSXSwitchedDevice
	virtual byte readSwitchedIO(word port, const EmuTime& time);
	virtual byte peekSwitchedIO(word port, const EmuTime& time) const;
	virtual void writeSwitchedIO(word port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<Rom> rom;
	unsigned adr;
};

} // namespace openmsx

#endif
