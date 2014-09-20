#ifndef ROMMAJUTSUSHI_HH
#define ROMMAJUTSUSHI_HH

#include "RomKonami.hh"

namespace openmsx {

class DACSound8U;

class RomMajutsushi final : public RomKonami
{
public:
	RomMajutsushi(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomMajutsushi();

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
