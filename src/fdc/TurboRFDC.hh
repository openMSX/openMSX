#ifndef TURBORFDC_HH
#define TURBORFDC_HH

#include "MSXFDC.hh"
#include "RomBlockDebuggable.hh"
#include "TC8566AF.hh"
#include <span>

namespace openmsx {

class TurboRFDC final : public MSXFDC
{
public:
	enum class Type { BOTH, R7FF2, R7FF8 };

	explicit TurboRFDC(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

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
