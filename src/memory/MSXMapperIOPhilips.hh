// $Id$

#ifndef MSXMAPPERIOPHILIPS_HH
#define MSXMAPPERIOPHILIPS_HH

#include "MSXMapperIO.hh"

namespace openmsx {

class MSXMapperIOPhilips : public MapperMask
{
public:
	virtual byte calcMask(const std::multiset<unsigned>& mapperSizes);

private:
	byte log2RoundedUp(unsigned num);
};

} // namespace openmsx

#endif

