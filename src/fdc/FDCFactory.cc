// $Id$

#include "FDCFactory.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"
#include "MSXCPUInterface.hh"


MSXDevice* FDCFactory::create(MSXConfig::Device *config, const EmuTime &time)
{
	const std::string &type = config->getParameter("type");
	if (type == "Philips") {
		return new PhilipsFDC(config, time);
	}
	if (type == "Microsol") {
		MicrosolFDC *fdc = new MicrosolFDC(config, time);
		MSXCPUInterface::instance()->register_IO_Out(0xD0, fdc);
		MSXCPUInterface::instance()->register_IO_Out(0xD1, fdc);
		MSXCPUInterface::instance()->register_IO_Out(0xD2, fdc);
		MSXCPUInterface::instance()->register_IO_Out(0xD3, fdc);
		MSXCPUInterface::instance()->register_IO_Out(0xD4, fdc);
		MSXCPUInterface::instance()->register_IO_In(0xD0, fdc);
		MSXCPUInterface::instance()->register_IO_In(0xD1, fdc);
		MSXCPUInterface::instance()->register_IO_In(0xD2, fdc);
		MSXCPUInterface::instance()->register_IO_In(0xD3, fdc);
		MSXCPUInterface::instance()->register_IO_In(0xD4, fdc);
		return fdc;
	}
	if (type == "National") {
		return new NationalFDC(config, time);
	}
	if (type == "TurboR") {
		return new TurboRFDC(config, time);
	}
	PRT_ERROR("Unknown FDC type");
	return NULL;
}
