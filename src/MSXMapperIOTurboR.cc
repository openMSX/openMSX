// $Id$

#include "MSXMapperIOTurboR.hh"


MSXMapperIOTurboR::MSXMapperIOTurboR(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMapperIOPhilips(config, time)
{
}

void  MSXMapperIOTurboR::registerMapper(int blocks)
{
	MSXMapperIOPhilips::registerMapper(blocks);
	mask |= 0xe0;
}
