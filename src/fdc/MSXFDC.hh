// $Id$

#ifndef __MSXFDC_HH__
#define __MSXFDC_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"

namespace openmsx {

class DiskDrive;

class MSXFDC : public MSXMemDevice
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	MSXFDC(const XMLElement& config, const EmuTime& time);
	virtual ~MSXFDC();

	Rom rom;
	DiskDrive* drives[4];
};

} // namespace openmsx
#endif
