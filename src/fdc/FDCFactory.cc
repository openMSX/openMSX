// $Id$

#include "Config.hh"
#include "FDCFactory.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"
#include "MSXCPUInterface.hh"

namespace openmsx {

MSXDevice* FDCFactory::create(Config* config, const EmuTime& time)
{
	const string& type = config->getParameter("fdc_type");
	if (type == "WD2793") {
		return new PhilipsFDC(config, time);
	}
	if (type == "Microsol") {
		MicrosolFDC* fdc = new MicrosolFDC(config, time);
		MSXCPUInterface::instance().register_IO_Out(0xD0, fdc);
		MSXCPUInterface::instance().register_IO_Out(0xD1, fdc);
		MSXCPUInterface::instance().register_IO_Out(0xD2, fdc);
		MSXCPUInterface::instance().register_IO_Out(0xD3, fdc);
		MSXCPUInterface::instance().register_IO_Out(0xD4, fdc);
		MSXCPUInterface::instance().register_IO_In(0xD0, fdc);
		MSXCPUInterface::instance().register_IO_In(0xD1, fdc);
		MSXCPUInterface::instance().register_IO_In(0xD2, fdc);
		MSXCPUInterface::instance().register_IO_In(0xD3, fdc);
		MSXCPUInterface::instance().register_IO_In(0xD4, fdc);
		return fdc;
	}
	if (type == "MB8877A") {
		return new NationalFDC(config, time);
	}
	if (type == "TC8566AF") {
		return new TurboRFDC(config, time);
	}
	throw FatalError("Unknown FDC type \"" + type + "\"!");
}

} // namespace openmsx
