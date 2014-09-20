#ifndef TURBORFDC_HH
#define TURBORFDC_HH

#include "MSXFDC.hh"
#include <memory>

namespace openmsx {

class TC8566AF;
class RomBlockDebuggable;

class TurboRFDC final : public MSXFDC
{
public:
	enum Type { BOTH, R7FF2, R7FF8 };

	explicit TurboRFDC(const DeviceConfig& config);
	~TurboRFDC();

	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte value);

	const std::unique_ptr<TC8566AF> controller;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	const byte* memory;
	const byte blockMask;
	byte bank;
	const Type type;
};

} // namespace openmsx

#endif
