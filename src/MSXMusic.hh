// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#include "MSXRom.hh"
#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "EmuTime.hh"
#include "YM2413.hh"


class MSXMusic : public MSXIODevice, public MSXMemDevice, public MSXRom
{
	public:
		/**
		 * Constructor
		 */
		MSXMusic(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXMusic(); 
		
		void reset(const EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);

	protected:
		void writeRegisterPort(byte value, EmuTime &time);
		void writeDataPort(byte value, EmuTime &time);

		byte enable;
		YM2413 *ym2413;
		
	private:
		int registerLatch;
};
#endif
