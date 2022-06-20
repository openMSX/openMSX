#ifndef MSXYAMAHASFG_HH
#define MSXYAMAHASFG_HH

#include "MSXDevice.hh"
#include "YM2151.hh"
#include "YM2148.hh"
#include "Rom.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXYamahaSFG final : public MSXDevice
{
public:
	explicit MSXYamahaSFG(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;
	[[nodiscard]] byte readIRQVector() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeRegisterPort(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);

private:
	Rom rom;
	YM2151 ym2151;
	YM2148 ym2148;
	int registerLatch;
	byte irqVector;
	byte irqVector2148;
};
SERIALIZE_CLASS_VERSION(MSXYamahaSFG, 2);

} // namespace openmsx

#endif
