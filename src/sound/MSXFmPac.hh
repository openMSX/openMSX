#ifndef MSXFMPAC_HH
#define MSXFMPAC_HH

#include "MSXMusic.hh"
#include "SRAM.hh"
#include "RomBlockDebuggable.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXFmPac final : public MSXMusicBase
{
public:
	explicit MSXFmPac(DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkSramEnable();

private:
	SRAM sram;
	RomBlockDebuggable romBlockDebug;
	byte enable;
	byte bank;
	byte r1ffe, r1fff;
	bool sramEnabled;
};
SERIALIZE_CLASS_VERSION(MSXFmPac, 3); // must be in-sync with MSXMusicBase

} // namespace openmsx

#endif
