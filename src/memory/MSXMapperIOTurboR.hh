// $Id$

#ifndef MSXMAPPERIOTURBOR_HH
#define MSXMAPPERIOTURBOR_HH

#include "MSXMapperIOPhilips.hh"

namespace openmsx {

class MSXMapperIOTurboR : public MSXMapperIOPhilips
{
public:
	virtual byte calcMask(const std::multiset<unsigned>& mapperSizes);
};

} // namespace openmsx

#endif
