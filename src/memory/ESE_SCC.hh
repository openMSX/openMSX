#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "MSXDevice.hh"
#include "SRAM.hh"
#include "SCC.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class MB89352;

class ESE_SCC final : public MSXDevice
{
public:
	ESE_SCC(const DeviceConfig& config, bool withSCSI);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getSramSize(bool withSCSI) const;
	void setMapperLow(unsigned page, byte value);
	void setMapperHigh(byte value);

private:
	SRAM sram;
	SCC scc;
	const std::unique_ptr<MB89352> spc; // can be nullptr
	RomBlockDebuggable romBlockDebug;

	const byte mapperMask;
	byte mapper[4];
	bool spcEnable;
	bool sccEnable;
	bool writeEnable;
};

} // namespace openmsx

#endif
