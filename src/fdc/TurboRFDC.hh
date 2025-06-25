#ifndef TURBORFDC_HH
#define TURBORFDC_HH

#include "MSXFDC.hh"
#include "RomBlockDebuggable.hh"
#include "TC8566AF.hh"

#include <cstdint>
#include <span>

namespace openmsx {

class TurboRFDC final : public MSXFDC
{
public:
	enum class Type : uint8_t { BOTH, R7FF2, R7FF8 };

	explicit TurboRFDC(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte value);

private:
	TC8566AF controller;
	RomBlockDebuggable romBlockDebug;
	std::span<const byte, 0x4000> memory;
	const byte blockMask;
	byte bank;
	const Type type;
};

} // namespace openmsx

#endif
