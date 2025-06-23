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
	explicit MSXMegaRam(DeviceConfig& config);
	~MSXMegaRam() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value,
	              EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

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
