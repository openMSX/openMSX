// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXIODevice.hh"
#include "MSXRom.hh"
#include "MSXDevice.hh"


class MSXKanji : public MSXIODevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXKanji(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXKanji();
		
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
		
		void reset(const EmuTime &time);

	private:
		static const int ROM_SIZE = 256*1024;
		
		int adr1, count1;
		int adr2, count2;
};

#endif //__MSXKANJI_HH__

