// $Id$

#ifndef __MSXBUNSETSU_HH__
#define __MSXBUNSETSU_HH__

#include "MSXMemDevice.hh"
#include "Rom.hh"


class MSXBunsetsu : public MSXMemDevice
{
	public:
		MSXBunsetsu(Device *config, const EmuTime &time);
		virtual ~MSXBunsetsu();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual const byte* getReadCacheLine(word start) const;

	private:
		Rom rom;
		Rom jisyoRom;
		int jisyoAddress;
};
#endif
