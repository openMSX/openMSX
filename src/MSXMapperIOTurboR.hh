// $Id$

#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIOPhilips.hh"

class MSXMapperIOTurboR : public MSXMapperIOPhilips
{
	public:
		MSXMapperIOTurboR(MSXConfig::Device *config, const EmuTime &time);
		void registerMapper(int blocks);
};

#endif //__MSXMAPPERIOTURBOR_HH__

