#ifndef ROMASCII16_2_HH
#define ROMASCII16_2_HH

#include "RomAscii16kB.hh"

#include <cstdint>

namespace openmsx {

class RomAscii16_2 final : public RomAscii16kB
{
public:
	enum class SubType : uint8_t { ASCII16_2, ASCII16_8 };
	RomAscii16_2(const DeviceConfig& config, Rom&& rom, SubType subType);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte sramEnabled;
};

} // namespace openmsx

#endif
