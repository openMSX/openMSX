// $Id$

#include "FDCFactory.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"


MSXDevice* FDCFactory::create(MSXConfig::Device *config, const EmuTime &time)
{
	const std::string &type = config->getParameter("type");
	if (type == "Philips") {
		return new PhilipsFDC(config, time);
	}
	if (type == "Microsol") {
		return new MicrosolFDC(config, time);
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
