// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXIODevice.hh"
#include "MSXRom.hh"


class MSXKanji : public MSXIODevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXKanji(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXKanji();
		
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
		
		void init();
		void reset();
	
	private:
		static const int ROM_SIZE = 256*1024;
		
		byte* buffer;
		int adr1, count1;
		int adr2, count2;
};

#endif //__MSXKANJI_HH__

