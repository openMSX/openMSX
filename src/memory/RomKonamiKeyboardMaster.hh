#ifndef ROMKONAMIKEYBOARDMASTER_HH
#define ROMKONAMIKEYBOARDMASTER_HH

#include "RomBlocks.hh"
#include "VLM5030.hh"

namespace openmsx {

class RomKonamiKeyboardMaster final : public Rom16kBBlocks
{
public:
	RomKonamiKeyboardMaster(const DeviceConfig& config, Rom&& rom);
	~RomKonamiKeyboardMaster() override;

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	VLM5030 vlm5030;
};

} // namespace openmsx

#endif
