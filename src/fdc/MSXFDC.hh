// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include <memory>
#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class DiskDrive;

class MSXFDC : public MSXDevice
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	MSXFDC(const XMLElement& config, const EmuTime& time);
	virtual ~MSXFDC();

	Rom rom;
	std::auto_ptr<DiskDrive> drives[4];
};

} // namespace openmsx
#endif
