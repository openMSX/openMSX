// $Id$

#ifndef __ROM16KBBLOCKS_HH__
#define __ROM16KBBLOCKS_HH__

#include "MSXRom.hh"


class Rom16kBBlocks : public MSXRom
{
	public:
		Rom16kBBlocks(Device* config, const EmuTime &time, Rom *rom);
		virtual ~Rom16kBBlocks() = 0;

		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;

	protected:
		void setBank(byte region, byte* adr);
		void setRom(byte region, int block);

		byte* bank[4];
};

#endif
