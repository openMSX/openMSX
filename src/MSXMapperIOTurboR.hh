// $Id$

#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIOPhilips.hh"


class MSXMapperIOTurboR : public MSXMapperIOPhilips
{
	public:
		virtual byte calcMask(std::list<int> &mapperSizes);
};

#endif //__MSXMAPPERIOTURBOR_HH__

