// $Id$

#ifndef __TURBORFDC_HH__
#define __TURBORFDC_HH__

#include "MSXFDC.hh"
#include <memory>

namespace openmsx {

class TC8566AF;

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
	const std::auto_ptr<TC8566AF> controller;
	const byte* memory;
	byte blockMask;
};

} // namespace openmsx
#endif
