// $Id$

#ifndef PHILIPSFDC_HH
#define PHILIPSFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class PhilipsFDC : public WD2793BasedFDC
{
public:
	PhilipsFDC(const XMLElement& config, const EmuTime& time);
	virtual ~PhilipsFDC();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

private:
	bool brokenFDCread;
	byte sideReg;
	byte driveReg;
};

} // namespace openmsx

#endif
