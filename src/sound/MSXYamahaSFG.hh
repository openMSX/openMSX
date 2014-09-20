#ifndef MSXYAMAHASFG_HH
#define MSXYAMAHASFG_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;
class YM2151;
class YM2148;

class MSXYamahaSFG final : public MSXDevice
{
public:
	explicit MSXYamahaSFG(const DeviceConfig& config);
	~MSXYamahaSFG();

	void reset(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	const byte* getReadCacheLine(word start) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word start) const override;
	byte readIRQVector() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeRegisterPort(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);

	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<YM2151> ym2151;
	const std::unique_ptr<YM2148> ym2148;
	int registerLatch;
	byte irqVector;
};

} // namespace openmsx

#endif
