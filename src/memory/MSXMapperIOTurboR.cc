// $Id$

#include "MSXMapperIOTurboR.hh"


byte MSXMapperIOTurboR::calcMask(std::list<int> &mapperSizes)
{
	// upper 3 bits are always "1"
	return MSXMapperIOPhilips::calcMask(mapperSizes) | 0xe0;
}
