// $Id$

#include "FDCFactory.hh"
#include "PhilipsFDC.hh"
#include "BrazilFDC.hh"
#include "NationalFDC.hh"


MSXDevice* FDCFactory::create(MSXConfig::Device *config, const EmuTime &time)
{
	const std::string &type = config->getParameter("type");
	if (type == "Philips") {
		return new PhilipsFDC(config, time);
	}
	if (type == "Brazil") {
		return new BrazilFDC(config, time);
	}
	if (type == "National") {
		return new NationalFDC(config, time);
	}
	PRT_ERROR("Unknown FDC type");
	return NULL;
}
