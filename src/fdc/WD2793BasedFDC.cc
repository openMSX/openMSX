// $Id$

#include "WD2793BasedFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(MSXMotherBoard& motherBoard,
                               const XMLElement& config, const EmuTime& time)
	: MSXFDC(motherBoard, config, time)
	, multiplexer(new DriveMultiplexer(reinterpret_cast<DiskDrive**>(drives)))
	, controller(new WD2793(*multiplexer, time))
{
}

WD2793BasedFDC::~WD2793BasedFDC()
{
}

void WD2793BasedFDC::reset(const EmuTime& time)
{
	controller->reset(time);
}

} // namespace openmsx
