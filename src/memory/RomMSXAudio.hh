// $Id$

#ifndef __ROMMSXAUDIO_HH__
#define __ROMMSXAUDIO_HH__

#include "MSXRom.hh"


class RomMSXAudio : public MSXRom
{
	public:
		RomMSXAudio(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomMSXAudio();

		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;

	protected:
		byte bankSelect;
		byte* ram;
};

#endif
