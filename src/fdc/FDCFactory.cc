// $Id$

#include "xmlx.hh"
#include "FDCFactory.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"
#include "MSXCPUInterface.hh"

namespace openmsx {

auto_ptr<MSXDevice> FDCFactory::create(const XMLElement& config,
                                       const EmuTime& time)
{
	const string& type = config.getChildData("fdc_type");
	if (type == "WD2793") {
		return auto_ptr<MSXDevice>(new PhilipsFDC(config, time));
	}
	if (type == "Microsol") {
		return auto_ptr<MSXDevice>(new MicrosolFDC(config, time));
	}
	if (type == "MB8877A") {
		return auto_ptr<MSXDevice>(new NationalFDC(config, time));
	}
	if (type == "TC8566AF") {
		return auto_ptr<MSXDevice>(new TurboRFDC(config, time));
	}
	throw FatalError("Unknown FDC type \"" + type + "\"!");
}

} // namespace openmsx
