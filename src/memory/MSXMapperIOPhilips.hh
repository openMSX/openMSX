// $Id$

#ifndef __MSXMAPPERIOPHILIPS_HH__
#define __MSXMAPPERIOPHILIPS_HH__

#include "MSXMapperIO.hh"

namespace openmsx {

class MSXMapperIOPhilips : public MapperMask
{
public:
	virtual byte calcMask(const multiset<unsigned>& mapperSizes);

private:
	byte log2RoundedUp(unsigned num);
};

} // namespace openmsx

#endif //__MSXMAPPERIOPHILIPS_HH__

