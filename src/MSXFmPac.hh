// $Id$

#ifndef __MSXFMPAC_HH__
#define __MSXFMPAC_HH__

#include "MSXMusic.hh"


class MSXFmPac : public MSXMusic
{
	public:
		/**
		 * Constructor
		 */
		MSXFmPac(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXFmPac(); 
		
		void reset(const EmuTime &time);
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);

	private:
		void checkSramEnable();
	
		bool sramEnabled;
		byte bank;
		byte r5ffe, r5fff;
		byte* sramBank;
};
#endif


