#ifndef SUNRISEIDE_HH
#define SUNRISEIDE_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class IDEDevice;
class Rom;
class RomBlockDebuggable;

class SunriseIDE final : public MSXDevice
{
public:
	explicit SunriseIDE(const DeviceConfig& config);
	virtual ~SunriseIDE();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);

	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeControl(byte value);

	byte readDataLow(EmuTime::param time);
	byte readDataHigh(EmuTime::param time);
	word readData(EmuTime::param time);
	byte readReg(nibble reg, EmuTime::param time);
	void writeDataLow(byte value);
	void writeDataHigh(byte value, EmuTime::param time);
	void writeData(word value, EmuTime::param time);
	void writeReg(nibble reg, byte value, EmuTime::param time);

	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
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
