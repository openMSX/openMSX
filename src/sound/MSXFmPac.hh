// $Id$

#ifndef __MSXFMPAC_HH__
#define __MSXFMPAC_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXYM2413.hh"
#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"


class MSXFmPac : public MSXYM2413, public MSXMemDevice, public MSXRomDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXFmPac(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXFmPac(); 
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);

	private:
		void checkSramEnable();
		
		static const char* PAC_Header;
	
		bool sramEnabled;
		byte bank;
		byte r5ffe, r5fff;
		byte* sramBank;
};

#endif


