#ifndef CANONWORDPROCESSOR_HH
#define CANONWORDPROCESSOR_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class CanonWordProcessor final : public MSXDevice
{
public:
	explicit CanonWordProcessor(DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine (uint16_t start) const override;
	[[nodiscard]]       byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] const byte* readHelper(uint16_t address) const;

private:
	Rom programRom;
	Rom dictionaryRom;
	byte select;
};

} // namespace openmsx

#endif
