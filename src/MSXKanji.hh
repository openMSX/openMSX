// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXDevice.hh"


class MSXKanji : public MSXDevice
{
	public:
		MSXKanji();
		~MSXKanji();
		
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
		
		void init();
		void reset();
	
	private:
		static const int ROM_SIZE = 131072;
		
		byte* buffer;
		int adr, count;
};

#endif //__MSXKANJI_HH__

