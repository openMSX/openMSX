#ifndef SUNRISEIDE_HH
#define SUNRISEIDE_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "RomBlockDebuggable.hh"

#include <memory>
#include <span>

namespace openmsx {

using uint4_t = uint8_t;

class IDEDevice;

class SunriseIDE final : public MSXDevice
{
public:
	explicit SunriseIDE(DeviceConfig& config);
	~SunriseIDE() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime::param time) override;
	void writeMem(uint16_t address, uint8_t value, EmuTime::param time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint8_t getBank() const;
	void writeControl(uint8_t value);

	[[nodiscard]] uint8_t readDataLow(EmuTime::param time);
	[[nodiscard]] uint8_t readDataHigh(EmuTime::param time) const;
	[[nodiscard]] uint16_t readData(EmuTime::param time);
	[[nodiscard]] uint8_t readReg(uint4_t reg, EmuTime::param time);
	void writeDataLow(uint8_t value);
	void writeDataHigh(uint8_t value, EmuTime::param time);
	void writeData(uint16_t value, EmuTime::param time);
	void writeReg(uint4_t reg, uint8_t value, EmuTime::param time);

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(const SunriseIDE& device);
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} romBlockDebug;

	Rom rom;
	std::array<std::unique_ptr<IDEDevice>, 2> device;
	std::span<const uint8_t, 0x4000> internalBank;
	uint8_t readLatch;
	uint8_t writeLatch;
	uint8_t selectedDevice;
	uint8_t control;
	bool ideRegsEnabled = false;
	bool softReset;
};

} // namespace openmsx

#endif
