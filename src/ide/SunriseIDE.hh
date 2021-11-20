#ifndef SUNRISEIDE_HH
#define SUNRISEIDE_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "RomBlockDebuggable.hh"
#include <memory>

namespace openmsx {

class IDEDevice;

class SunriseIDE final : public MSXDevice
{
public:
	explicit SunriseIDE(const DeviceConfig& config);
	~SunriseIDE() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] byte getBank() const;
	void writeControl(byte value);

	[[nodiscard]] byte readDataLow(EmuTime::param time);
	[[nodiscard]] byte readDataHigh(EmuTime::param time) const;
	[[nodiscard]] word readData(EmuTime::param time);
	[[nodiscard]] byte readReg(nibble reg, EmuTime::param time);
	void writeDataLow(byte value);
	void writeDataHigh(byte value, EmuTime::param time);
	void writeData(word value, EmuTime::param time);
	void writeReg(nibble reg, byte value, EmuTime::param time);

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(SunriseIDE& device);
		[[nodiscard]] byte read(unsigned address) override;
	} romBlockDebug;

	Rom rom;
	std::unique_ptr<IDEDevice> device[2];
	const byte* internalBank;
	byte readLatch;
	byte writeLatch;
	byte selectedDevice;
	byte control;
	bool ideRegsEnabled;
	bool softReset;
};

} // namespace openmsx

#endif
