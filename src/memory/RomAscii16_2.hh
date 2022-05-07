#ifndef ROMASCII16_2_HH
#define ROMASCII16_2_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class RomAscii16_2 final : public RomAscii16kB
{
public:
	enum SubType { ASCII16_2, ASCII16_8 };
	RomAscii16_2(const DeviceConfig& config, Rom&& rom, SubType subType);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	byte sramEnabled;
};

} // namespace openmsx

#endif
