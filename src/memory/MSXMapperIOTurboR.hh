// $Id$

#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIOPhilips.hh"


namespace openmsx {

class MSXMapperIOTurboR : public MSXMapperIOPhilips
{
	public:
		virtual byte calcMask(list<int> &mapperSizes);
};

} // namespace openmsx

#endif //__MSXMAPPERIOTURBOR_HH__

