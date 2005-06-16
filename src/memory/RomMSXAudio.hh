// $Id$

#ifndef ROMMSXAUDIO_HH
#define ROMMSXAUDIO_HH

#include "MSXRom.hh"

namespace openmsx {

class Ram;

class RomMSXAudio : public MSXRom
{
public:
	RomMSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomMSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

protected:
	const std::auto_ptr<Ram> ram;
	byte bankSelect;
};

} // namespace openmsx

#endif
