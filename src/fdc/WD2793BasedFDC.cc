// $Id$

#include "WD2793BasedFDC.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(const XMLElement& config, const EmuTime& time)
	: MSXFDC(config, time)
	, multiplexer(reinterpret_cast<DiskDrive**>(drives))
	, controller(&multiplexer, time)
{
}

WD2793BasedFDC::~WD2793BasedFDC()
{
}

void WD2793BasedFDC::reset(const EmuTime& time)
{
	controller.reset(time);
}

} // namespace openmsx
