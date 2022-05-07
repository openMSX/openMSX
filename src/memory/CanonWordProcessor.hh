#ifndef CANONWORDPROCESSOR_HH
#define CANONWORDPROCESSOR_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class CanonWordProcessor : public MSXDevice
{
public:
	explicit CanonWordProcessor(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine (word start) const override;
	[[nodiscard]]       byte* getWriteCacheLine(word start) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	[[nodiscard]] const byte* readHelper(word address) const;

private:
	Rom programRom;
	Rom dictionaryRom;
	byte select;
};

} // namespace openmsx

#endif
