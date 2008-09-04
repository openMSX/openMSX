// $Id$

#ifndef MSXFDC_HH
#define MSXFDC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;
class DiskDrive;

class MSXFDC : public MSXDevice
{
public:
	virtual void powerDown(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	MSXFDC(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXFDC();

	const std::auto_ptr<Rom> rom;
	std::auto_ptr<DiskDrive> drives[4];
};

REGISTER_BASE_NAME_HELPER(MSXFDC, "FDC");

} // namespace openmsx

#endif
