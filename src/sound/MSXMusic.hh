// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "Rom.hh"

class YM2413;


class MSXMusic : public MSXIODevice, MSXMemDevice
{
	public:
		MSXMusic(Device *config, const EmuTime &time);
		virtual ~MSXMusic();
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;
	
	protected:
		void writeRegisterPort(byte value, const EmuTime &time);
		void writeDataPort(byte value, const EmuTime &time);

		Rom rom;
		YM2413 *ym2413;

	private:
		int registerLatch;
};

#endif
