
#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIO.hh"

class MSXMapperIOTurboR : public MSXMapperIO
{
	public:
		MSXMapperIOTurboR();
		~MSXMapperIOTurboR();
		byte readIO(byte port, Emutime &time);
		void registerMapper(int blocks);
};

#endif //__MSXMAPPERIOTURBOR_HH__

