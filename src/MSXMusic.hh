// $Id: MSXMusic.hh,v

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#include "MSXDevice.hh"
#include "EmuTime.hh"
#include "YM2413.hh"


class MSXMusic : public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMusic(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXMusic(); 
		
		void init();
		void reset();
		void writeIO(byte port, byte value, EmuTime &time);
	private:
		int registerLatch;
		YM2413 *ym2413;
	
};
#endif
