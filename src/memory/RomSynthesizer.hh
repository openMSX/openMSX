// $Id$

#ifndef ROMSYNTHESIZER_HH
#define ROMSYNTHESIZER_HH

#include "Rom16kBBlocks.hh"

namespace openmsx {

class DACSound8U;

class RomSynthesizer : public Rom16kBBlocks
{
public:
	RomSynthesizer(const XMLElement& config, const EmuTime& time,
	               std::auto_ptr<Rom> rom);
	virtual ~RomSynthesizer();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
