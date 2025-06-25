#ifndef TALENTTDC600_HH
#define TALENTTDC600_HH

#include "MSXFDC.hh"
#include "TC8566AF.hh"

namespace openmsx {

class TalentTDC600 final : public MSXFDC
{
public:
	explicit TalentTDC600(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	TC8566AF controller;
};

} // namespace openmsx

#endif
