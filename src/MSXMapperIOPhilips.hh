// $Id$

#ifndef __MSXMAPPERIOPHILIPS_HH__
#define __MSXMAPPERIOPHILIPS_HH__

#include "MSXMapperIO.hh"

class MSXMapperIOPhilips : public MSXMapperIODevice
{
	public:
		MSXMapperIOPhilips();
		byte convert(byte value);
		void registerMapper(int blocks);

	private:
		int log2RoundedUp(int num);
		int largest;
		byte mask;
};

#endif //__MSXMAPPERIOPHILIPS_HH__

