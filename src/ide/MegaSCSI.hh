#ifndef MEGASCSI_HH
#define MEGASCSI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class MB89352;
class SRAM;
class RomBlockDebuggable;

class MegaSCSI final : public MSXDevice
{
public:
	explicit MegaSCSI(const DeviceConfig& config);
	~MegaSCSI();

	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setSRAM(unsigned region, byte block);

	const std::unique_ptr<MB89352> mb89352;
	const std::unique_ptr<SRAM> sram;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // SPC block mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
