// $Id$

#include "MSXMapperIOTurboR.hh"

void  MSXMapperIOTurboR::registerMapper(int blocks)
{
	MSXMapperIOPhilips::registerMapper(blocks);
	mask |= 0xe0;
}
