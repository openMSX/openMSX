#ifndef TALENTTDC600_HH
#define TALENTTDC600_HH

#include "MSXFDC.hh"
#include "TC8566AF.hh"

namespace openmsx {

class TalentTDC600 final : public MSXFDC
{
public:
	explicit TalentTDC600(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	TC8566AF controller;
};

} // namespace openmsx

#endif
