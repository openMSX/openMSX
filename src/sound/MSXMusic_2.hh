// $Id$

#ifndef __MSXMUSIC_2_HH__
#define __MSXMUSIC_2_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "Rom.hh"

class YM2413_2;


class MSXMusic_2 : public MSXIODevice, public MSXMemDevice
{
	public:
		MSXMusic_2(Device *config, const EmuTime &time);
		virtual ~MSXMusic_2();
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;
	
	protected:
		void writeRegisterPort(byte value, const EmuTime &time);
		void writeDataPort(byte value, const EmuTime &time);

		Rom rom;
		YM2413_2 *ym2413;

	private:
		int registerLatch;
};

#endif
