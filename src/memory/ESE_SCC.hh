#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "MSXDevice.hh"
#include "RomBlockDebuggable.hh"
#include <memory>

namespace openmsx {

class SRAM;
class SCC;
class MB89352;

class ESE_SCC final : public MSXDevice
{
public:
	ESE_SCC(const DeviceConfig& config, bool withSCSI);
	~ESE_SCC();

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setMapperLow(unsigned page, byte value);
	void setMapperHigh(byte value);

	const std::unique_ptr<SRAM> sram;
	const std::unique_ptr<SCC> scc;
	const std::unique_ptr<MB89352> spc;
	RomBlockDebuggable romBlockDebug;

	const byte mapperMask;
	byte mapper[4];
	bool spcEnable;
	bool sccEnable;
	bool writeEnable;
};

} // namespace openmsx

#endif
