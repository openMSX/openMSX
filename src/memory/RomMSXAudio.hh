// $Id$

#ifndef __ROMMSXAUDIO_HH__
#define __ROMMSXAUDIO_HH__

#include "MSXRom.hh"
#include "Ram.hh"

namespace openmsx {

class RomMSXAudio : public MSXRom
{
public:
	RomMSXAudio(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomMSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

protected:
	byte bankSelect;
	Ram ram;
};

} // namespace openmsx

#endif
