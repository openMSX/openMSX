// $Id$

#ifndef __ROMSYNTHESIZER_HH__
#define __ROMSYNTHESIZER_HH__

#include <memory>
#include "Rom16kBBlocks.hh"

using std::auto_ptr;

namespace openmsx {

class DACSound8U;

class RomSynthesizer : public Rom16kBBlocks
{
public:
	RomSynthesizer(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomSynthesizer();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
