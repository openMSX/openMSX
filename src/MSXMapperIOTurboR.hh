// $Id$

#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIOPhilips.hh"

class MSXMapperIOTurboR : public MSXMapperIOPhilips
{
	public:
		void registerMapper(int blocks);
};

#endif //__MSXMAPPERIOTURBOR_HH__

