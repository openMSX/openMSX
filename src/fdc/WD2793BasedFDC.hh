// $Id$

#ifndef __WD2793BASEDFDC_HH__
#define __WD2793BASEDFDC_HH__

#include "MSXFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"

namespace openmsx {

class WD2793BasedFDC : public MSXFDC
{
public:
	virtual void reset(const EmuTime& time);
	
protected:
	WD2793BasedFDC(const XMLElement& config, const EmuTime& time);
	virtual ~WD2793BasedFDC();
	
	DriveMultiplexer multiplexer;
	WD2793 controller;
};

} // namespace openmsx

#endif
