// $Id$

#ifndef __ROMSYNTHESIZER_HH__
#define __ROMSYNTHESIZER_HH__

#include "Rom16kBBlocks.hh"


class RomSynthesizer : public Rom16kBBlocks
{
	public:
		RomSynthesizer(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomSynthesizer();

		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
	
	private:
		class DACSound8U* dac;
};

#endif
