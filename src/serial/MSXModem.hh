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
	explicit MSXModem(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	SRAM sram;
	uint8_t bank;
	byte selectedNCUreg;
};

} // namespace openmsx

#endif
