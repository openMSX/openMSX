// $Id$

#ifndef __MSXFMPAC_HH__
#define __MSXFMPAC_HH__

#ifndef VERSION
#include "config.h"
#endif

#ifndef DONT_WANT_FMPAC

#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"
#include "MSXRom.hh"


class MSXFmPac : public MSXYM2413, public MSXMemDevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXFmPac(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXFmPac(); 
		
		void reset(const EmuTime &time);
		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);

	private:
		void checkSramEnable();
		
		static const unsigned char* PAC_Header;
	
		bool sramEnabled;
		byte bank;
		byte r5ffe, r5fff;
		byte* sramBank;
};

#endif // ndef DONT_WANT_FMPAC

#endif


