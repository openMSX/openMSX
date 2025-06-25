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
	explicit MSXYamahaSFG(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;
	[[nodiscard]] byte readIRQVector() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeRegisterPort(byte value, EmuTime time);
	void writeDataPort(byte value, EmuTime time);

private:
	Rom rom;
	YM2151 ym2151;
	YM2148 ym2148;
	byte registerLatch;
	byte irqVector;
	byte irqVector2148;
};
SERIALIZE_CLASS_VERSION(MSXYamahaSFG, 2);

} // namespace openmsx

#endif
