#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "RomBlockDebuggable.hh"
#include "SRAM.hh"

#include "MSXDevice.hh"
#include "SCC.hh"

#include <array>

namespace openmsx {

class MB89352;

class ESE_SCC final : public MSXDevice
{
public:
	ESE_SCC(DeviceConfig& config, bool withSCSI);

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] size_t getSramSize(bool withSCSI) const;
	void setMapperLow(unsigned page, byte value);
	void setMapperHigh(byte value);

private:
	SRAM sram;
	SCC scc;
	const std::unique_ptr<MB89352> spc; // can be nullptr
	RomBlockDebuggable romBlockDebug;

	const byte mapperMask;
	std::array<byte, 4> mapper;
	bool spcEnable = false;
	bool sccEnable = false;
	bool writeEnable = false;
};

} // namespace openmsx

#endif
