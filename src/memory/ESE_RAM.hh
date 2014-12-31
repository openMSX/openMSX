#ifndef ESE_RAM_HH
#define ESE_RAM_HH

#include "MSXDevice.hh"
#include "RomBlockDebuggable.hh"
#include <memory>

namespace openmsx {

class SRAM;

class ESE_RAM final : public MSXDevice
{
public:
	ESE_RAM(const DeviceConfig& config);
	~ESE_RAM();

	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setSRAM(unsigned region, byte block);

	const std::unique_ptr<SRAM> sram;
	RomBlockDebuggable romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // which block is mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
