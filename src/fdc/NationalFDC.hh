// $Id$

#ifndef NATIONALFDC_HH
#define NATIONALFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class NationalFDC : public WD2793BasedFDC
{
public:
	NationalFDC(const XMLElement& config, const EmuTime& time);
	virtual ~NationalFDC();
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

private:
	byte driveReg;
};

} // namespace openmsx

#endif
