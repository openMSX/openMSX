// $Id$

#ifndef __ROM4KBBLOCKS_HH__
#define __ROM4KBBLOCKS_HH__

#include "MSXRom.hh"


class Rom4kBBlocks : public MSXRom
{
	public:
		Rom4kBBlocks(Device* config, const EmuTime &time, Rom *rom);
		virtual ~Rom4kBBlocks();

		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;

	protected:
		void setBank(byte region, byte* adr);
		void setRom(byte region, int block);

		byte* bank[16];
};

#endif
