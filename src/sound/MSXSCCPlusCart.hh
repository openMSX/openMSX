// Note: this device is actually called SCC-I. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#ifndef MSXSCCPLUSCART_HH
#define MSXSCCPLUSCART_HH

#include "MSXDevice.hh"
#include "SCC.hh"
#include "Ram.hh"
#include "RomBlockDebuggable.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class MSXSCCPlusCart final : public MSXDevice
{
public:
	struct MapperConfig {
		unsigned numBlocks;  // number of 8kB blocks
		byte registerMask;   // mapper selection bits to ignore
		byte registerOffset; // first mapped block
	};

public:
	explicit MSXSCCPlusCart(const DeviceConfig& config);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setMapper(int region, byte value);
	void setModeRegister(byte value);
	void checkEnable();

private:
	const MapperConfig mapperConfig;
	Ram ram;
	SCC scc;
	RomBlockDebuggable romBlockDebug;
	std::array<byte*, 4> internalMemoryBank; // 4 blocks of 8kB starting at #4000
	enum SCCEnable : uint8_t {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
	byte modeRegister;
	std::array<bool, 4> isRamSegment;
	std::array<bool, 4> isMapped;
	std::array<byte, 4> mapper;
};

} // namespace openmsx

#endif
