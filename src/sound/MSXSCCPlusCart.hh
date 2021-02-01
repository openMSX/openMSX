// Note: this device is actually called SCC-I. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#ifndef MSXSCCPLUSCART_HH
#define MSXSCCPLUSCART_HH

#include "MSXDevice.hh"
#include "SCC.hh"
#include "Ram.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class MSXSCCPlusCart final : public MSXDevice
{
public:
	explicit MSXSCCPlusCart(const DeviceConfig& config);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
        [[nodiscard]] unsigned getRamSize() const;

	void setMapper(int region, byte value);
	void setModeRegister(byte value);
	void checkEnable();

private:
	Ram ram;
	SCC scc;
	RomBlockDebuggable romBlockDebug;
	byte* internalMemoryBank[4]; // 4 blocks of 8kB starting at #4000
	enum SCCEnable {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
	byte modeRegister;
	bool isRamSegment[4];
	bool isMapped[4];
	byte mapper[4];
	/*const*/ byte mapperMask;
	/*const*/ bool lowRAM, highRAM;
};

} // namespace openmsx

#endif
