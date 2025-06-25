#ifndef ROMKONAMIKEYBOARDMASTER_HH
#define ROMKONAMIKEYBOARDMASTER_HH

#include "RomBlocks.hh"
#include "VLM5030.hh"

namespace openmsx {

class RomKonamiKeyboardMaster final : public Rom16kBBlocks
{
public:
	RomKonamiKeyboardMaster(DeviceConfig& config, Rom&& rom);
	~RomKonamiKeyboardMaster() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	VLM5030 vlm5030;
};

} // namespace openmsx

#endif
