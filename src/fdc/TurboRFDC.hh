#ifndef TURBORFDC_HH
#define TURBORFDC_HH

#include "MSXFDC.hh"
#include "RomBlockDebuggable.hh"
#include "TC8566AF.hh"

namespace openmsx {

class TurboRFDC final : public MSXFDC
{
public:
	enum Type { BOTH, R7FF2, R7FF8 };

	explicit TurboRFDC(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	void setBank(byte value);

private:
	TC8566AF controller;
	RomBlockDebuggable romBlockDebug;
	const byte* memory;
	const byte blockMask;
	byte bank;
	const Type type;
};

} // namespace openmsx

#endif
