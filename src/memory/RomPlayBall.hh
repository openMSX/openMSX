// $Id$

#ifndef ROMPLAYBALL_HH
#define ROMPLAYBALL_HH

#include "RomBlocks.hh"

namespace openmsx {

class SamplePlayer;

class RomPlayBall : public Rom16kBBlocks
{
public:
	RomPlayBall(MSXMotherBoard& motherBoard, const XMLElement& config,
	            std::auto_ptr<Rom> rom);
	virtual ~RomPlayBall();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	std::auto_ptr<SamplePlayer> samplePlayer;
};

} // namespace openmsx

#endif
