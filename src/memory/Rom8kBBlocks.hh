// $Id$

#ifndef __ROM8KBBLOCKS_HH__
#define __ROM8KBBLOCKS_HH__

#include "MSXRom.hh"


class Rom8kBBlocks : public MSXRom
{
	public:
		Rom8kBBlocks(Device* config, const EmuTime &time);
		virtual ~Rom8kBBlocks();

		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;

	protected:
		void setBank(byte region, byte* adr);
		void setRom(byte region, int block);

		byte* bank[8];
};

#endif
