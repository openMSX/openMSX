// $Id$

#ifndef __TURBORFDC_HH__
#define __TURBORFDC_HH__

#include "MSXFDC.hh"
#include "TC8566AF.hh"

namespace openmsx {

class TurboRFDC : public MSXFDC
{
public:
	TurboRFDC(const XMLElement& config, const EmuTime& time);
	virtual ~TurboRFDC();
	
	virtual void reset(const EmuTime& time);
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;

private:
	byte blockMask;
	const byte* memory;
	TC8566AF controller;
};

} // namespace openmsx
#endif
