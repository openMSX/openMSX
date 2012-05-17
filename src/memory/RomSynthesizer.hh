// $Id$

#ifndef ROMSYNTHESIZER_HH
#define ROMSYNTHESIZER_HH

#include "RomBlocks.hh"

namespace openmsx {

class DACSound8U;

class RomSynthesizer : public Rom16kBBlocks
{
public:
	RomSynthesizer(MSXMotherBoard& motherBoard, const DeviceConfig& config,
	               std::auto_ptr<Rom> rom);
	virtual ~RomSynthesizer();

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
