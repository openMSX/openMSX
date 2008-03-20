// $Id$

#ifndef MSXROM_HH
#define MSXROM_HH

#include "MSXDevice.hh"
#include "RomTypes.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXRom : public MSXDevice
{
public:
	virtual ~MSXRom();

	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	void setRomType(RomType type);
	virtual void getExtraDeviceInfo(TclObject& result) const;

protected:
	MSXRom(MSXMotherBoard& motherBoard, const XMLElement& config,
	       std::auto_ptr<Rom> rom);

	const std::auto_ptr<Rom> rom;

private:
	RomType type;
};

} // namespace openmsx

#endif
