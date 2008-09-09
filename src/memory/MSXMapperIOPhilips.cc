// $Id$

#include "MSXMapperIOPhilips.hh"
#include "Math.hh"

namespace openmsx {

// unused bits read always "1"
byte MSXMapperIOPhilips::calcMask(const std::multiset<unsigned>& mapperSizes)
{
	unsigned largest = (mapperSizes.empty()) ? 1 : *mapperSizes.rbegin();
	return (256 - Math::powerOfTwo(largest)) & 255;
}

} // namespace openmsx
