// $Id$

#ifndef __MSXMAPPERIOPHILIPS_HH__
#define __MSXMAPPERIOPHILIPS_HH__

#include "MSXMapperIO.hh"

class MSXMapperIOPhilips : public MSXMapperIO
{
	public:
		MSXMapperIOPhilips(MSXConfig::Device *config, const EmuTime &time);
		byte convert(byte value);
		void registerMapper(int blocks);

	protected:
		byte mask;
		
	private:
		int log2RoundedUp(int num);
		int largest;
};

#endif //__MSXMAPPERIOPHILIPS_HH__

