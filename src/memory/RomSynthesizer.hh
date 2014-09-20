#ifndef ROMSYNTHESIZER_HH
#define ROMSYNTHESIZER_HH

#include "RomBlocks.hh"

namespace openmsx {

class DACSound8U;

class RomSynthesizer final : public Rom16kBBlocks
{
public:
	RomSynthesizer(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomSynthesizer();

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
