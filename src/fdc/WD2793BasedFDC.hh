// $Id$

#ifndef WD2793BASEDFDC_HH
#define WD2793BASEDFDC_HH

#include "MSXFDC.hh"
#include <memory>

namespace openmsx {

class DriveMultiplexer;
class WD2793;

class WD2793BasedFDC : public MSXFDC
{
public:
	virtual void reset(const EmuTime& time);
	
protected:
	WD2793BasedFDC(const XMLElement& config, const EmuTime& time);
	virtual ~WD2793BasedFDC();
	
	const std::auto_ptr<DriveMultiplexer> multiplexer;
	const std::auto_ptr<WD2793> controller;
};

} // namespace openmsx

#endif
