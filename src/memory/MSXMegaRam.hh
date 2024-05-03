#ifndef MSXMEGARAM_HH
#define MSXMEGARAM_HH

#include "MSXDevice.hh"
#include "Ram.hh"
#include "RomBlockDebuggable.hh"
#include <array>
#include <memory>

namespace openmsx {

class Rom;

class MSXMegaRam final : public MSXDevice
{
public:
	explicit MSXMegaRam(const DeviceConfig& config);
	~MSXMegaRam() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value,
	              EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte page, byte block);

private:
	const unsigned numBlocks; // must come before ram
	Ram ram;
	const std::unique_ptr<Rom> rom; // can be nullptr
	RomBlockDebuggable romBlockDebug;
	const byte maskBlocks;
	std::array<byte, 4> bank;
	bool writeMode;
	bool romMode;
};

} // namespace openmsx

#endif
