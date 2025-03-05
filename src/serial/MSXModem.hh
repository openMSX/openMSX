#ifndef MSXMODEM_HH
#define MSXMODEM_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include <array>

// PLEASE NOTE!
//
// This device is work in progress. It's just a guess based on some reverse
// engineering of the ROM by Zeilemaker54. It even contains some debug prints :)
//
// Perhaps this can be used for hardware like the modem in the Sony HB-T600,
// HBI-1200 and maybe even Mitsubishi ML-TS2(H).
//
// The actual modem part isn't implemented at all!

namespace openmsx {

class MSXModem final : public MSXDevice
{
public:
	explicit MSXModem(DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeMem(uint16_t address, uint8_t value, EmuTime::param time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] uint8_t* getWriteCacheLine(uint16_t address) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	SRAM sram;
	uint8_t bank;
	uint8_t selectedNCUreg;
};

} // namespace openmsx

#endif
