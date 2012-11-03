// $Id$

#ifndef ROMNETTOUYAKYUU_HH
#define ROMNETTOUYAKYUU_HH

#include "RomBlocks.hh"

namespace openmsx {

class SamplePlayer;

class RomNettouYakyuu : public Rom8kBBlocks
{
public:
	RomNettouYakyuu(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<SamplePlayer> samplePlayer;

	// remember per region if writes are for the sample player or not
	// there are 4 x 8kB regions in [0x4000-0xBFFF]
	bool redirectToSamplePlayer[4];
};

} // namespace openmsx

#endif
