// $Id$

#ifndef __MSXMAPPERIOPHILIPS_HH__
#define __MSXMAPPERIOPHILIPS_HH__

#include "MSXMapperIO.hh"


class MSXMapperIOPhilips : public MapperMask
{
	public:
		virtual byte calcMask(list<int> &mapperSizes);

	private:
		int log2RoundedUp(int num);
};

#endif //__MSXMAPPERIOPHILIPS_HH__

