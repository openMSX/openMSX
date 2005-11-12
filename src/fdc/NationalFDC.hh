// $Id$

#ifndef NATIONALFDC_HH
#define NATIONALFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class NationalFDC : public WD2793BasedFDC
{
public:
	NationalFDC(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

private:
	byte driveReg;
};

} // namespace openmsx

#endif
