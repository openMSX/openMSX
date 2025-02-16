#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "MSXDevice.hh"
#include "SRAM.hh"
#include "SCC.hh"
#include "RomBlockDebuggable.hh"
#include <array>

namespace openmsx {

class MB89352;

class ESE_SCC final : public MSXDevice
{
public:
	ESE_SCC(DeviceConfig& config, bool withSCSI);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

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
