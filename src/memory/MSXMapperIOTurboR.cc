// $Id$

#include "MSXMapperIOTurboR.hh"

namespace openmsx {

byte MSXMapperIOTurboR::calcMask(const multiset<unsigned>& mapperSizes)
{
	// upper 3 bits are always "1"
	return MSXMapperIOPhilips::calcMask(mapperSizes) | 0xe0;
}

} // namespace openmsx
