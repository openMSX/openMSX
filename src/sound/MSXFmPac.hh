#ifndef MSXFMPAC_HH
#define MSXFMPAC_HH

#include "MSXMusic.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class SRAM;
class RomBlockDebuggable;

class MSXFmPac final : public MSXMusic
{
public:
	explicit MSXFmPac(const DeviceConfig& config);
	~MSXFmPac();

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkSramEnable();

	const std::unique_ptr<SRAM> sram;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	byte enable;
	byte bank;
	byte r1ffe, r1fff;
	bool sramEnabled;
};
SERIALIZE_CLASS_VERSION(MSXFmPac, 2); // must be in-sync with MSXMusic

} // namespace openmsx

#endif
