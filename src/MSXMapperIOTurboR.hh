// $Id$

#ifndef __MSXMAPPERIOTURBOR_HH__
#define __MSXMAPPERIOTURBOR_HH__

#include "MSXMapperIO.hh"

class MSXMapperIOTurboR : public MSXMapperIODevice
{
	public:
		byte convert(byte value);
		void registerMapper(int blocks);
};

#endif //__MSXMAPPERIOTURBOR_HH__

