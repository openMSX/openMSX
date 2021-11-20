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
	explicit MSXFmPac(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

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
